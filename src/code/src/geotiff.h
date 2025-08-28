//
// Created by User on 2025/08/23.
//

#ifndef GEOTIFF_H
#define GEOTIFF_H

#include "scalarfield2.h"
#include "box2.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "scalarfield2.h"

struct RasterBand {
    std::vector<double> data = {};
    double minValue = 0;
    double maxValue = 1.0;
    int index = 1;
};

struct Raster {
    std::unordered_map<std::string, RasterBand> bands;
    int nx;
    int ny;
    Vector2 pixel_scale;
    GDALDatasetH gdal_handle;
    Raster() : nx(0), ny(0), pixel_scale(1.0, 1.0), gdal_handle(nullptr) {}
    ~Raster()
    {
        if (gdal_handle) {
            GDALClose(gdal_handle);
        }
    }
};

static bool gdal_initialized = false;

inline int read_geotiff(const char* filepath, Raster& raster, double scaling_options[4] = nullptr)
{
    if (!gdal_initialized) {
        GDALAllRegister();
        gdal_initialized = true;
    }

    auto dataset = GDALOpen(filepath, GA_ReadOnly );
    if (dataset == nullptr) {
        return 0;
    }
    double geotransform[6];

    CPLErr err = GDALGetGeoTransform(dataset, &geotransform[0]);
    if (err != CE_None) {
        return 0;
    }

    int band_count = GDALGetRasterCount(dataset);

    for (int i = 1; i <= band_count; i++) { // bands are 1-indexed
        auto band = GDALGetRasterBand(dataset, i);
        int nx = GDALGetRasterXSize(dataset);
        int ny = GDALGetRasterYSize(dataset);


        // GDALFillNodata(band, nullptr, 3, 0, 1, nullptr, nullptr, nullptr);

        RasterBand raster_band;
        raster_band.data.resize(nx * ny);

        err = GDALRasterIO(band,
                        GF_Read,
                        0, 0,
                        nx, ny,
                        &raster_band.data[0],
                        nx, ny,
                        GDT_Float64,
                        sizeof(double), sizeof(double)*nx);

        if (err != CE_None) {
            return 0;
        }

        double minmax[2];
        err = GDALComputeRasterMinMax(band, FALSE, &minmax[0]);
        if (err != CE_None) {
            return 0;
        }

        int success;
        auto scale = GDALGetRasterScale(band, &success);
        if (!success) {
            scale = 1.0;
        }

        auto offset = GDALGetRasterOffset(band, &success);
        if (!success) {
            offset = 0.0;
        }

        minmax[0] = minmax[0] * scale + offset;
        minmax[1] = minmax[1] * scale + offset;

        // double valid_max = minmax[1] - minmax[0];
        // double valid_min = 0.0f;
        auto no_data = GDALGetRasterNoDataValue(band, &success);
        if (!success) {
            no_data = -9999.0;
        }

        for (auto &d : raster_band.data) {
            if (d == no_data) {
                d = 0.0;
            }
        }

        if (scaling_options != nullptr) {
            double srcmin = scaling_options[0];
            double srcmax = scaling_options[1];
            double newmin = scaling_options[2];
            double newmax = scaling_options[3];

            for (auto &d : raster_band.data) {
                d = (d - srcmin) / (srcmax - srcmin) * (newmax - newmin) + newmin;
            }
            minmax[0] = newmin;
            minmax[1] = newmax;
        }

        // normalize


        GDALDatasetUniquePtr poDataset;
        std::string name = GDALGetDescription(band);
        size_t pos = name.find_last_of("_");
        if (pos != std::string::npos) {
            name = name.substr(0, pos);
        }
        std::cout << name << std::endl;
        // auto item = GDALGetMetadataItem(band, "Description", nullptr);
        // std::cout << item << std::endl;
        // std ::cout << GDALGetMetadataItem(band, "Description", nullptr) << std::endl;

        raster.gdal_handle = dataset;
        raster.nx = nx;
        raster.ny = ny;
        raster.pixel_scale = Vector2(abs(geotransform[1]), abs(geotransform[5]));

        raster_band.minValue = minmax[0];
        raster_band.maxValue = minmax[1];
        raster_band.index = i;
        // raster.bands.push_back(raster_band);
        raster.bands[name] = raster_band;
    }
    return 1;
}

inline Box2 raster_to_box2(const Raster& raster)
{
    return Box2(
        Vector2::Null,
        raster.pixel_scale[0] * double(raster.nx) /2.0
        );
}

inline void load_raster_to_field( Raster& raster, ScalarField2& field, const char* band, double range[2] = nullptr)
{
    // std::cout << "Loading raster to field with size: " << raster.nx << " x " << raster.ny << std::endl;
    // std::cout << "Pixel scale: " << raster.pixel_scale[0] << ", " << raster.pixel_scale[1] << std::endl;
    // std::cout << "Box: " << raster_to_box2(raster) << std::endl;
    double default_range[2] {raster.bands[band].minValue, raster.bands[band].maxValue};
    if (range == nullptr) {
        range = default_range;
    }



    field = ScalarField2(
        raster_to_box2(raster),
        raster.nx,
        raster.ny,
        raster.bands[band].data
        );
    // field.SetRange(range[0], range[1]);
    std::cout << "Field loaded" << std::endl;
    std::cout << "Field size: " << field.GetBox() <<  std::endl;
    double frange[2];
    field.GetRange(frange[0], frange[1]);
    std::cout << "Field range: " << frange[0] << ", "<< frange[1] << std::endl;

}

inline void save_field_to_raster( const ScalarField2& field, Raster& raster_template, const char* band, const char* fullpath, const double scale[4] = nullptr)
{
    if (!gdal_initialized) {
        GDALAllRegister();
        gdal_initialized = true;
    }
    // clone dataset and clear clone's bands
    auto dataset_handle = raster_template.gdal_handle;

    std::vector<std::string> translate_options;

    int band_no = raster_template.bands[std::string(band)].index;
    translate_options.push_back("-b"); translate_options.push_back(std::to_string(band_no));

    translate_options.push_back("-outsize");
    translate_options.push_back(std::to_string(field.GetSizeX()));
    translate_options.push_back(std::to_string(field.GetSizeY()));

    std::vector<char*> cstrings;
    for(size_t i = 0; i < translate_options.size(); ++i)
        cstrings.push_back(const_cast<char*>(translate_options[i].c_str()));
    cstrings.push_back(nullptr);

    GDALTranslateOptions* options = GDALTranslateOptionsNew(&cstrings[0], nullptr);

    auto out_handle = GDALTranslate(fullpath, dataset_handle, options, nullptr);
    GDALTranslateOptionsFree(options);

    if (out_handle == nullptr) {
        std::cout << "Failed to create output dataset" << std::endl;
        return;
    }

    std::cout << "saving raster to " << fullpath << std::endl;

    Vector2 pixel_scale =   raster_template.nx /double(field.GetSizeX()) * raster_template.pixel_scale;

    std::cout << "Pixel scale: " << pixel_scale[0] << ", " << pixel_scale[1] << std::endl;

    double geotransform[6];
    GDALGetGeoTransform(out_handle, &geotransform[0]);

    std::cout << "Original geotransform: ";
    std::cout << geotransform[0] << ", " << geotransform[1] << ", " << geotransform[2] << "\n"
              << geotransform[3] << ", " << geotransform[4] << ", " << geotransform[5] << std::endl;

    geotransform[1] = pixel_scale[0];
    geotransform[5] = -pixel_scale[1];
    std::cout << "New geotransform: ";
    std::cout << geotransform[0] << ", " << geotransform[1] << ", " << geotransform[2] << "\n"
              << geotransform[3] << ", " << geotransform[4] << ", " << geotransform[5] << std::endl;

    GDALSetGeoTransform(out_handle, geotransform);

    int nx = field.GetSizeX();
    int ny = field.GetSizeY();



    auto out_band = GDALGetRasterBand(out_handle, 1);

    CPLErr err = GDALRasterIO(out_band, GF_Write, 0, 0,
                             nx, ny,
                             field.GetData().data(), nx, ny,
                             GDT_Float64, sizeof(double), sizeof(double) * nx);
    if (err != CE_None) {
        std::cout << "Failed to write raster data" << std::endl;
    } else {
        std::cout << "Raster data written" << std::endl;
    }
    if (scale==nullptr) {
        double min, max;
        field.GetRange(min, max);
        GDALSetRasterStatistics(out_band, min, max, field.Average(), 0);
        if (err != CE_None) {
            std::cout << "Failed to set raster statistics" << std::endl;
        }
    }
    GDALClose(out_handle);
}

inline void map_raster_range(double oldmin, double oldmax, double newmin, double newmax, Raster& raster, const char* band)
{
    double old_range = oldmax - oldmin;
    double new_range = newmax - newmin;
    for (auto &d : raster.bands[band].data) {
        d = (d - oldmin) / old_range * new_range + newmin;
        d = std::clamp(d, newmin, newmax);
    }
    raster.bands[band].minValue = newmin;
    raster.bands[band].maxValue = newmax;
}


#endif //GEOTIFF_H
