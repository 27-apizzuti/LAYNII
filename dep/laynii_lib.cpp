
#include "./laynii_lib.h"

// ============================================================================
// Command-line log messages
// ============================================================================

void log_welcome(const char* programname) {
    cout << "============="<< endl;
    cout << "LAYNII v1.5.0"<< endl;
    cout << "============="<< endl;
    cout << programname << "\n" << endl;
}

void log_output(const char* filename) {
    cout << "  Writing output as:" << endl;
    cout << "    " << filename << endl;
}

void log_nifti_descriptives(nifti_image* nii) {
    // Print nifti descriptives to command line for debugging
    cout << "  File name: " << nii->fname << endl;
    cout << "    Image details: " << nii->nz << " Z | " << nii->nx
         << " X | " << nii->ny << " Y | " << nii->nt << " T " << endl;
    cout << "    Voxel size = " << nii->pixdim[1] << " x " << nii->pixdim[2]
         << " x " << nii->pixdim[3] << endl;
    cout << "    Datatype = " << nii->datatype << "\n" << endl;
}

// ============================================================================
// Statistics functions
// ============================================================================

double ren_average(double arr[], int size) {
    int i;
    double sum = 0;
    double avg;

    for (i = 0; i < size; ++i) {
        sum += arr[i];
    }
    avg = double(sum) / size;
    return avg;
}

double ren_stdev(double arr[], int size) {
    int i;
    double sum = 0;
    double mean;

    mean = ren_average(arr, size);

    for (i = 0; i < size; ++i) {
        sum += (arr[i] - mean) * (arr[i] - mean) / ((double)size - 1);
    }
    return sqrt( sum);
}

double ren_correl(double arr1[], double arr2[], int size) {
    int i;
    double sum1 = 0;
    double sum2 = 0;
    double sum3 = 0;
    double mean1 = ren_average(arr1, size);
    double mean2 = ren_average(arr2, size);

    for (i = 0; i < size; ++i) {
        sum1 += (arr1[i] - mean1) * (arr2[i] - mean2);
        sum2 += (arr1[i] - mean1) * (arr1[i] - mean1);
        sum3 += (arr2[i] - mean2) * (arr2[i] - mean2);
    }
    return sum1 / sqrt(sum2 * sum3);
}

double ren_skew(double arr[], int size) {
    int i;
    double sum1 = 0;
    double sum2 = 0;
    double mean = ren_average(arr, size);

    for (i = 0; i < size; ++i) {
        sum1 += (arr[i] - mean) * (arr[i] - mean) * (arr[i] - mean);
        sum2 += (arr[i] - mean) * (arr[i] - mean);
    }
    return ((1 / ((double)size) * sum1) / (pow(1 / ((double)size - 1) * sum2, 1.5)));
}

double ren_kurt(double arr[], int size) {
    int i;
    double sum1 = 0;
    double sum2 = 0;
    double mean = ren_average(arr, size);

    for (i = 0; i < size; ++i) {
        sum1 += (arr[i] - mean) * (arr[i] - mean) * (arr[i] - mean) * (arr[i] - mean) / ((double)size);
        sum2 += (arr[i] - mean) * (arr[i] - mean) / ((double)size);
    }
    return sum1 / (sum2 * sum2) - 3;
}

double ren_autocor(double arr[], int size) {
    int i;
    double sum1 = 0;
    double sum2 = 0;
    double mean = ren_average(arr, size);

    for (i = 1; i < size; ++i) {
        sum1 += (arr[i]-mean)*(arr[i-1]-mean);
    }
    for (i = 0; i < size; ++i) {
        sum2 += (arr[i]-mean)*(arr[i]-mean);
    }
    return sum1/sum2;
}

float dist(float x1, float y1, float z1, float x2, float y2, float z2,
           float dX, float dY, float dZ) {
    return sqrt(pow((x1 - x2) * dX, 2) + pow((y1 - y2) * dY, 2)
                + pow((z1 - z2) * dZ, 2));
}

float dist2d(float x1, float y1, float x2, float y2) {
    return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

float angle(float a, float b, float c) {
    if (a * a + b * b - c * c <= 0) {
        return 3.141592;
    } else {
        return acos((a * a + b * b - c * c) / (2. * a * b));
    }
}

float gaus(float distance, float sigma) {
    return 1. / (sigma * sqrt(2. * 3.141592))
           * exp(-0.5 * distance * distance / (sigma * sigma));
}

// ============================================================================
// Utility functions
// ============================================================================

void save_output_nifti(const string path, const string tag,
                       nifti_image* nii, const bool log) {
    // Parse path
    string dir, file, basename, ext, sep;
    auto pos1 = path.find_last_of('/');
    if (pos1 != string::npos) {  // For Unix
        sep = "/";
        dir = path.substr(0, pos1);
        file = path.substr(pos1 + 1);
    } else {  // For Windows
        pos1 = path.find_last_of('\\');
        if (pos1 != string::npos) {
            sep = "\\";
            dir = path.substr(0, pos1);
            file = path.substr(pos1 + 1);
        } else {  // Only the filename
            sep = "";
            dir = "";
            file = path;
        }
    }

    // Parse extension
    auto const pos2 = file.find_first_of('.');
    basename = file.substr(0, pos2);
    ext = file.substr(pos2);

    // Prepare output path
    string out_path = dir + sep + basename + "_" + tag + ext;

    // Save nifti
    nifti_set_filenames(nii, out_path.c_str(), 1, 1);
    nifti_image_write(nii);
    if (log) {
        log_output(out_path.c_str());
    }
}

nifti_image* copy_nifti_as_float32(nifti_image* nii) {
    ///////////////////////////////////////////////////////////////////////////
    // NOTE(Renzo): Fixing potential problems with different input datatypes //
    // here, I am loading them in their native datatype and cast them        //
    // to the datatype I like best.                                          //
    ///////////////////////////////////////////////////////////////////////////

    // NOTE(for future reference): Rick's comments:
    // nifti_copy_nim_info(). It will return with data == NULL.
    // If you need the data allocated, memory use would not change once you do
    // so. There is also nifti_make_new_nim()
    // nifti_image* nifti_make_new_nim(const int64_t dims[],
    //                                 int datatype, int data_fill)

    nifti_image* nii_new = nifti_copy_nim_info(nii);
    nii_new->datatype = NIFTI_TYPE_FLOAT32;
    nii_new->nbyper = sizeof(float);
    nii_new->data = calloc(nii_new->nvox, nii_new->nbyper);
    float* nii_new_data = static_cast<float*>(nii_new->data);

    // NOTE(Faruk): See nifti1.h for notes on data types
    // ------------------------------------------------------------------------
    if (nii->datatype == 2) {  // NIFTI_TYPE_UINT8
        uint8_t* nii_data = static_cast<uint8_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 512) {  // NIFTI_TYPE_UINT16
        uint16_t* nii_data = static_cast<uint16_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 768) {  // NIFTI_TYPE_UINT32
        uint32_t* nii_data = static_cast<uint32_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 1280) {  // NIFTI_TYPE_UINT64
        uint64_t* nii_data = static_cast<uint64_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 256) {  // NIFTI_TYPE_INT8
        int8_t* nii_data = static_cast<int8_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 4) {  // NIFTI_TYPE_INT16
        int16_t* nii_data = static_cast<int16_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 8) {  // NIFTI_TYPE_INT32
        int32_t* nii_data = static_cast<int32_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 1024) {  // NIFTI_TYPE_INT64
        int64_t* nii_data = static_cast<int64_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 16) {  // NIFTI_TYPE_FLOAT32
        float* nii_data = static_cast<float*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else if (nii->datatype == 64) {  // NIFTI_TYPE_FLOAT64
        double* nii_data = static_cast<double*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<float>(*(nii_data + i));
        }
    } else {
        cout << "Warning! Unrecognized nifti data type!" << endl;
    }

    // Replace nans with zeros
    for (int i = 0; i < nii->nvox; ++i) {
        if (isnan(*(nii_new_data + i))) {
            *(nii_new_data + i) = 0;
        }
    }

    return nii_new;
}

nifti_image* copy_nifti_as_int32(nifti_image* nii) {
    nifti_image* nii_new = nifti_copy_nim_info(nii);
    nii_new->datatype = NIFTI_TYPE_INT32;
    nii_new->nbyper = sizeof(int32_t);
    nii_new->data = calloc(nii_new->nvox, nii_new->nbyper);

    int32_t* nii_new_data = static_cast<int32_t*>(nii_new->data);

    // NOTE(Faruk): See nifti1.h for notes on data types
    // ------------------------------------------------------------------------
    if (nii->datatype == 2) {  // NIFTI_TYPE_UINT8
        uint8_t* nii_data = static_cast<uint8_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 512) {  // NIFTI_TYPE_UINT16
        uint16_t* nii_data = static_cast<uint16_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 768) {  // NIFTI_TYPE_UINT32
        uint32_t* nii_data = static_cast<uint32_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 1280) {  // NIFTI_TYPE_UINT64
        uint64_t* nii_data = static_cast<uint64_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 256) {  // NIFTI_TYPE_INT8
        int8_t* nii_data = static_cast<int8_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 4) {  // NIFTI_TYPE_INT16
        int16_t* nii_data = static_cast<int16_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 8) {  // NIFTI_TYPE_INT32
        int32_t* nii_data = static_cast<int32_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 1024) {  // NIFTI_TYPE_INT64
        int64_t* nii_data = static_cast<int64_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 16) {  // NIFTI_TYPE_FLOAT32
        float* nii_data = static_cast<float*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 64) {  // NIFTI_TYPE_FLOAT64
        double* nii_data = static_cast<double*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int32_t>(*(nii_data + i));
        }
    } else {
        cout << "Warning! Unrecognized nifti data type!" << endl;
    }

    // Replace nans with zeros
    for (int i = 0; i < nii->nvox; ++i) {
        if (isnan(*(nii_new_data + i))) {
            *(nii_new_data + i) = 0;
        }
    }

    return nii_new;
}

nifti_image* copy_nifti_as_int16(nifti_image* nii) {
    nifti_image* nii_new = nifti_copy_nim_info(nii);
    nii_new->datatype = NIFTI_TYPE_INT16;
    nii_new->nbyper = sizeof(int16_t);
    nii_new->data = calloc(nii_new->nvox, nii_new->nbyper);

    int16_t* nii_new_data = static_cast<int16_t*>(nii_new->data);

    // NOTE(Faruk): See nifti1.h for notes on data types
    // ------------------------------------------------------------------------
    if (nii->datatype == 2) {  // NIFTI_TYPE_UINT8
        uint8_t* nii_data = static_cast<uint8_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 512) {  // NIFTI_TYPE_UINT16
        uint16_t* nii_data = static_cast<uint16_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 768) {  // NIFTI_TYPE_UINT32
        uint32_t* nii_data = static_cast<uint32_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 1280) {  // NIFTI_TYPE_UINT64
        uint64_t* nii_data = static_cast<uint64_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 256) {  // NIFTI_TYPE_INT8
        int8_t* nii_data = static_cast<int8_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 4) {  // NIFTI_TYPE_INT16
        int16_t* nii_data = static_cast<int16_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 8) {  // NIFTI_TYPE_INT32
        int32_t* nii_data = static_cast<int32_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 1024) {  // NIFTI_TYPE_INT64
        int64_t* nii_data = static_cast<int64_t*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 16) {  // NIFTI_TYPE_FLOAT32
        float* nii_data = static_cast<float*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else if (nii->datatype == 64) {  // NIFTI_TYPE_FLOAT64
        double* nii_data = static_cast<double*>(nii->data);
        for (int i = 0; i < nii_new->nvox; ++i) {
            *(nii_new_data + i) = static_cast<int16_t>(*(nii_data + i));
        }
    } else {
        cout << "Warning! Unrecognized nifti data type!" << endl;
    }

    // Replace nans with zeros
    for (int i = 0; i < nii->nvox; ++i) {
        if (isnan(*(nii_new_data + i))) {
            *(nii_new_data + i) = 0;
        }
    }

    return nii_new;
}


// ============================================================================
// Faruk's favorite functions
// ============================================================================
std::tuple<uint32_t, uint32_t, uint32_t> ind2sub_3D(
    const uint32_t linear_index, const uint32_t size_x, const uint32_t size_y) {
    uint32_t z = linear_index / (size_x * size_y);
    uint32_t temp = linear_index % (size_x * size_y);
    uint32_t y = temp / size_x;
    uint32_t x = temp % size_x;
    return std::make_tuple(x, y, z);
}

uint32_t sub2ind_3D(const uint32_t x, const uint32_t y, const uint32_t z,
                    const uint32_t size_x, const uint32_t size_y) {
    return size_x * size_y * z + size_x * y + x;
}