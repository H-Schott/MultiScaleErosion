//
// Created by User on 2025/08/23.
//

#ifndef GEOTIFF_H
#define GEOTIFF_H

#include "scalarfield2.h"
#include "box2.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "scalarfield2.h"

struct RasterBand {
    std::vector<double> data = {};
    double minValue = 0;
    double maxValue = 1.0;
};

struct Raster {
    std::vector<RasterBand> bands;
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

inline int read_geotiff(const char* filepath, Raster& raster)
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

        GDALFillNodata(band, nullptr, 3, 0, 1, nullptr, nullptr, nullptr);

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
        // normalize
        for (auto &d : raster_band.data) {
            if (d == no_data) {
                d = 0.0;
                continue;
            }
            // d = d * scale + offset;
            // d = (d - minmax[0]) / (minmax[1]);
        }

        GDALDatasetUniquePtr poDataset;

        raster.gdal_handle = dataset;
        raster.nx = nx;
        raster.ny = ny;
        raster.pixel_scale = Vector2(abs(geotransform[1]), abs(geotransform[5]));

        raster_band.minValue = minmax[0];
        raster_band.maxValue = minmax[1];
        raster.bands.push_back(raster_band);
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

inline void load_raster_to_field( Raster& raster, ScalarField2& field, int band = 0, double range[2] = nullptr)
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

inline void map_raster_range(double oldmin, double oldmax, double newmin, double newmax, Raster& raster, int band = 0)
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

inline void load_soil_tex_raster_to_fields(Raster& raster, ScalarField2& siltf, ScalarField2& sandf, ScalarField2& clayf, ScalarField2& depthf)
{
    if (raster.bands.size() < 4) {
        std::cout << "Raster has less than 4 bands, cannot load soil texture." << std::endl;
        return;
    }
    siltf = ScalarField2(
        raster_to_box2(raster),
        raster.nx,
        raster.ny,
        raster.bands[0].data
        );
    siltf.SetRange(raster.bands[0].minValue, raster.bands[0].maxValue);

    sandf = ScalarField2(
        raster_to_box2(raster),
        raster.nx,
        raster.ny,
        raster.bands[1].data
        );
    sandf.SetRange(raster.bands[1].minValue, raster.bands[1].maxValue);

    clayf = ScalarField2(
        raster_to_box2(raster),
        raster.nx,
        raster.ny,
        raster.bands[2].data
        );
    clayf.SetRange(raster.bands[2].minValue, raster.bands[2].maxValue);

    depthf = ScalarField2(
        raster_to_box2(raster),
        raster.nx,
        raster.ny,
        raster.bands[3].data
        );
    depthf.SetRange(raster.bands[3].minValue, raster.bands[3].maxValue);
}

#endif //GEOTIFF_H
