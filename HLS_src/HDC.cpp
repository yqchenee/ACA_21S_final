#include "HCD.h"
#include "cmath"

#ifndef __SYNTHESIS__
    #include <iostream>
#endif

/**
 * Gaussian filter with sigma 1.5
 * **/
GRAY_PIXEL Gaussian_filter_15(WINDOW* window)
{
    char i,j;
    double sum = 0;
    GRAY_PIXEL pixel =0;

    const float op[3][3] = {
        {0.0947417, 0.118318, 0.0947417},
        {0.118318 , 0.147761, 0.118318},
        {0.0947417, 0.118318, 0.0947417}
    };

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            sum += window->getval(i,j) * op[i][j];
        }
    }
    pixel = round(sum);
    return pixel;
}

/**
 * Gaussian filter with sigma 1
 * **/
DOUBLE_PIXEL Gaussian_filter_1(DOUBLE_WINDOW* window)
{
    char i,j;
    double sum = 0;
    DOUBLE_PIXEL pixel =0;

    const float op[3][3] = {
        {0.0751136, 0.123841, 0.0751136},
        {0.123841, 0.20418, 0.123841},
        {0.0751136, 0.123841, 0.0751136}
    };

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            sum += window->getval(i,j) * op[i][j];
        }
    }
    pixel = round(sum);
    return pixel;
}

void process_input(AXI_PIXEL* input, GRAY_BUFFER* gray_buf, int32_t i, int32_t j)
{

    GRAY_PIXEL input_gray_pix;
    input_gray_pix = (input->data.range(7,0)
            + input->data.range(15,8)
            + input->data.range(23,16))/3;

    gray_buf->insert_at(input_gray_pix, i, j);
}

void blur_img(GRAY_BUFFER* gray_buf, GRAY_BUFFER* blur_buf, int32_t row, int32_t col)
{
    int32_t i;
    int32_t j;
    GRAY_PIXEL blur;
    GRAY_PIXEL zero = 0;
    WINDOW gray_window;


    for (i = 0; i < MAX_HEIGHT; ++i) {
    #pragma HLS loop_tripcount max=256
        gray_window.shift_right();
        gray_window.insert( (i == 0)? gray_buf-> getval(1, 1): gray_buf->getval(i-1, 1) , 0, 2);
        gray_window.insert(gray_buf->getval(i, 1), 1, 2);
        gray_window.insert( (i == MAX_HEIGHT-1)? gray_buf-> getval(MAX_HEIGHT-2, 1): gray_buf->getval(i+1, 1) , 2, 2);

        for (j = 0; j < MAX_WIDTH; ++j) {

            gray_window.shift_right();
	        gray_window.insert( (i == 0)? gray_buf-> getval(1, j): gray_buf->getval(i-1, j), 0, 2);
	        gray_window.insert(gray_buf->getval(i,j), 1, 2);
            gray_window.insert( (i == MAX_HEIGHT-1)? gray_buf-> getval(MAX_HEIGHT-2, j): gray_buf->getval(i+1, j) , 2, 2);

            if (j < 1) {
                continue;
            }

            blur = Gaussian_filter_15(&gray_window);
            blur_buf->insert_at(blur, i, j-1);
        }

        gray_window.shift_right();
        gray_window.insert( (i == 0)? gray_buf-> getval(1, MAX_WIDTH-2): gray_buf->getval(i-1, MAX_WIDTH-2), 0, 2);
        gray_window.insert(gray_buf->getval(i, MAX_WIDTH-2), 1, 2);
        gray_window.insert( (i == MAX_HEIGHT-1)? gray_buf-> getval(MAX_HEIGHT-2, MAX_WIDTH-2): gray_buf->getval(i+1, MAX_WIDTH-2) , 2, 2);
        blur = Gaussian_filter_15(&gray_window);
        blur_buf->insert_at(blur, i, MAX_WIDTH-1);
    }

#ifndef __SYNTHESIS__
    // for(int i = 250 ; i < MAX_HEIGHT ; i++) {
    //     for(int j = 250 ; j < MAX_WIDTH ; j++) {
    //         std::cout << blur_buf-> getval(i, j) << ' ' ;
    //     }
    //     std::cout << std::endl;
    // }
#endif
}


void compute_dif(GRAY_BUFFER* blur_buf, DOUBLE_BUFFER* Ixx_buf,
        DOUBLE_BUFFER* Iyy_buf, DOUBLE_BUFFER* Ixy_buf, int32_t row, int32_t col)
{
    int32_t i;
    int32_t j;
    DIF_PIXEL Ix, Iy, zero = 0;
    DOUBLE_PIXEL Ixx, Iyy, Ixy;

    for (i = 0; i < MAX_HEIGHT; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j)
        {
            Ix = (j > 0 && j < MAX_WIDTH-1) ? blur_buf-> getval(i, j-1) - blur_buf-> getval(i, j+1) : zero;
            Iy = (i > 0 && i < MAX_HEIGHT-1) ? blur_buf-> getval(i-1, j) - blur_buf-> getval(i+1, j) : zero;

            Ixx = Ix * Ix;
            Iyy = Iy * Iy;
            Ixy = Ix * Iy;

            Ixx_buf->insert_at(Ixx, i, j);
            Iyy_buf->insert_at(Iyy, i, j);
            Ixy_buf->insert_at(Ixy, i, j);
#ifndef __SYNTHESIS__
    // if(i < 5 && j < 5)
    // std::cout << i << ' ' << j << ' ' << Ix << ' ' << Iy << ' ' \
    //         << Ixx << ' ' << Ixy << ' ' << Iyy << std::endl;
#endif
        }
    }
}

void compute_sum(DOUBLE_BUFFER* Ixx_buf, DOUBLE_BUFFER* Ixy_buf,
        DOUBLE_BUFFER* Iyy_buf, TWICE_BUFFER* matrix, int32_t row, int32_t col)
{
    int32_t i;
    int32_t j;

    DOUBLE_PIXEL Sxx, Sxy, Syy;
    DOUBLE_WINDOW Ixx_window;
    DOUBLE_WINDOW Iyy_window;
    DOUBLE_WINDOW Ixy_window;

    for (i = 1; i < MAX_HEIGHT-1; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j) {
            Ixx_window.shift_right();
	        Ixx_window.insert(Ixx_buf->getval(i-1,j),0,2);
	        Ixx_window.insert(Ixx_buf->getval(i,j),1,2);
	        Ixx_window.insert(Ixx_buf->getval(i+1,j),2,2);

            if (j < 2) {
                continue;
            }
            Sxx = Gaussian_filter_1(&Ixx_window);
            matrix->insert_at(Sxx, i, j-1);
        }
    }

    for (i = 1; i < MAX_HEIGHT-1; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j) {
            Iyy_window.shift_right();
	        Iyy_window.insert(Iyy_buf->getval(i-1,j),0,2);
	        Iyy_window.insert(Iyy_buf->getval(i,j),1,2);
	        Iyy_window.insert(Iyy_buf->getval(i+1,j),2,2);

            if (j < 2) {
                continue;
            }
            Syy = Gaussian_filter_1(&Iyy_window);
            matrix->insert_at(Syy, MAX_HEIGHT + i, MAX_WIDTH + j-1);
        }
    }

    for (i = 1; i < MAX_HEIGHT-1; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j) {
            Ixy_window.shift_right();
	        Ixy_window.insert(Ixy_buf->getval(i-1,j),0,2);
	        Ixy_window.insert(Ixy_buf->getval(i,j),1,2);
	        Ixy_window.insert(Ixy_buf->getval(i+1,j),2,2);

            if (j < 2) {
                continue;
            }
            Sxy = Gaussian_filter_1(&Ixy_window);
            matrix->insert_at(Sxy, MAX_HEIGHT + i, j-1);
            matrix->insert_at(Sxy, i, MAX_WIDTH + j-1);
        }
    }
}

void compute_det_trace(TWICE_BUFFER* matrix, DOUBLE_BUFFER* det_buf,
        DOUBLE_BUFFER* trace_buf, int32_t row, int32_t col)
{
    int32_t i;
    int32_t j;
    DOUBLE_PIXEL Ixx, Ixy, Iyy, det;

    for (i = 0; i < MAX_HEIGHT; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j) {
            Ixx = matrix-> getval(i, j);
            Ixy = matrix-> getval(MAX_HEIGHT + i, j);
            Ixx = matrix-> getval(MAX_HEIGHT + i, MAX_WIDTH + j);
            // TODO: float precision
            det = Ixx * Iyy;
            trace_buf-> insert_at(det, i, j);
            // TODO: float precision
            det -= Ixy * Ixy;
            det_buf->insert_at(det, i, j);
#ifndef __SYNTHESIS__
    // std::cout << i << ' ' << j << ' ' << Ixx << ' ' << Ixy << ' ' << Iyy \
    //             << ' ' << trace_buf-> getval(i, j) << ' ' << det_buf-> getval(i, j) << std::endl;
#endif
        }
    }
}

void compute_response(DOUBLE_BUFFER* det_buf, DOUBLE_BUFFER* trace_buf,
        GRAY_BUFFER* response_buf, int32_t row, int32_t col)
{
    int32_t i;
    int32_t j;
    for (i = 0; i < MAX_HEIGHT; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j) {
            double response = double(det_buf-> getval(i, j)) / (double(trace_buf-> getval(i, j)) + 1e-12);
            response_buf->insert_at(round(response), i, j);
        }
    }
}

void output_maxima(GRAY_BUFFER* response_buf, stream_to& pstrmOutput, int32_t row, int32_t col)
{
    int32_t i;
    int32_t j;
    BOOL_PIXEL ret_pixel;
    for(i = 0 ; i < MAX_HEIGHT ; i++) {
        for(j = 0 ; j < MAX_WIDTH ; j++) {
            if(response_buf-> getval(i, j) > 100) {
                ret_pixel.data = 1;
            } else {
                ret_pixel.data = 0;
            }
            pstrmOutput.write(ret_pixel);
        }
    }
}

void HCD(stream_ti& pstrmInput, stream_to& pstrmOutput, reg32_t row, reg32_t col)
{
#pragma HLS INTERFACE axis register both port=pstrmOutput
#pragma HLS INTERFACE axis register both port=pstrmInput
#pragma HLS INTERFACE s_axilite port=row
#pragma HLS INTERFACE s_axilite port=col

#pragma dataflow

    int32_t i;
    int32_t j;
    AXI_PIXEL input;

    GRAY_BUFFER gray_buf;
    GRAY_BUFFER blur_buf;
    DOUBLE_BUFFER Ixx_buf;     // ap_int<9>
    DOUBLE_BUFFER Iyy_buf;     // ap_int<9>
    DOUBLE_BUFFER Ixy_buf;     // ap_int<9>
    DOUBLE_BUFFER det_buf;
    DOUBLE_BUFFER trace_buf;
    GRAY_BUFFER response_buf;

    TWICE_BUFFER matrix;

    for (i = 0; i < MAX_HEIGHT; ++i) {
        for (j = 0; j < MAX_WIDTH; ++j) {
            input = pstrmInput.read();
             // Step 0: Convert to gray scale
            process_input(&input, &gray_buf, i, j);
        }
    }

#ifndef __SYNTHESIS__
	std::cout << "step 0 input" << std::endl;
#endif

    // Step 1: Smooth the image by Gaussian kernel
    blur_img(&gray_buf, &blur_buf, row, col);

#ifndef __SYNTHESIS__
	std::cout << "step 1 smooth image" << std::endl;
#endif

    // Step 2: Calculate Ix, Iy (1st derivative of image along x and y axis)
    // Step 3: Compute Ixx, Ixy, Iyy (Ixx = Ix*Ix, ...)
    compute_dif(&blur_buf, &Ixx_buf, &Iyy_buf,
            &Ixy_buf, row, col);

#ifndef __SYNTHESIS__
	std::cout << "step 2, 3 compute diff" << std::endl;
#endif

    // Step 4: Compute Sxx, Sxy, Syy (weighted summation of Ixx, Ixy, Iyy in neighbor pixels)
    compute_sum(&Ixx_buf, &Ixy_buf, &Iyy_buf, &matrix, row, col);

#ifndef __SYNTHESIS__
	std::cout << "step 4 compute sum" << std::endl;
#endif

    // Todo:
    // Step 5: Compute the det and trace of matrix M (M = [[Sxx, Sxy], [Sxy, Syy]])
    compute_det_trace(&matrix, &det_buf, &trace_buf, row, col);

#ifndef __SYNTHESIS__
	std::cout << "step 5 compute det trace" << std::endl;
#endif

    // Step 6: Compute the response of the detector by det/(trace+1e-12)
    compute_response(&det_buf, &trace_buf, &response_buf, row, col);

#ifndef __SYNTHESIS__
	std::cout << "step 6 compute response" << std::endl;
#endif

    // Step 7: Post processing
    output_maxima(&response_buf, pstrmOutput, row, col);

#ifndef __SYNTHESIS__
	std::cout << "step 7 output" << std::endl;
#endif

}
