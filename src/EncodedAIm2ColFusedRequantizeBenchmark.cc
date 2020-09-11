/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "./BenchUtils.h"
#include "fbgemm/Fbgemm.h"
#include "src/RefImplementations.h"

using namespace std;
using namespace fbgemm;

template <typename Acc_t>
void performance_test() {
  vector<conv_param_t<>> shapes = {
    #if 1
      // MB, IC, OC, IH, IW, G, KH, KW, stride_h, stride_w,
      // pad_h_top, pad_w_left, pad_h_bottom, pad_w_right
      // ResNext 101
      // Batch size = 1
      conv_param_t<>(1, 3, 64, {224, 224}, 1, {7, 7}, {2, 2}, {3, 3, 3, 3}),
      conv_param_t<>(1, 3, 64, {320, 320}, 1, {7, 7}, {2, 2}, {3, 3, 3, 3}),
      conv_param_t<>(1, 3, 64, {320, 320}, 1, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(1, 128, 128, {56, 56}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(1, 128, 128, {56, 56}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(1, 256, 256, {56, 56}, 32, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(1, 256, 256, {28, 28}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(1, 512, 512, {28, 28}, 32, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(1, 512, 512, {14, 14}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(1, 512, 512, {14, 14}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(1, 1024, 1024, {14, 14}, 32, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(1, 1024, 1024, {7, 7}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(1, 1024, 1024, {7, 7}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      // Batch size = 50
      conv_param_t<>(50, 3, 64, {224, 224}, 1, {7, 7}, {2, 2}, {3, 3, 3, 3}),
      conv_param_t<>(50, 128, 128, {56, 56}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(50, 128, 128, {56, 56}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(50, 256, 256, {56, 56}, 32, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(50, 256, 256, {28, 28}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(50, 512, 512, {28, 28}, 32, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(50, 512, 512, {14, 14}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(50, 512, 512, {14, 14}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(
          50, 1024, 1024, {14, 14}, 32, {3, 3}, {2, 2}, {1, 1, 1, 1}),
      conv_param_t<>(50, 1024, 1024, {7, 7}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
      conv_param_t<>(50, 1024, 1024, {7, 7}, 32, {3, 3}, {1, 1}, {1, 1, 1, 1}),
    #endif
    //conv_param_t<>(1, 1, 1, {16, 16}, 1, {3, 3}, {1, 1}, {1, 1, 1, 1}),
    //conv_param_t<>(1, 3, 64, {224, 224}, 1, {7, 7}, {2, 2}, {3, 3, 3, 3}),
  };

  bool flush = true;
  std::vector<char> llc;

  if (flush) {
    llc.resize(128 * 1024 * 1024, 1.0);
  }

  constexpr int NWARMUP = 4;
  constexpr int NITER = 10;

#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
  cout << "WARNING: the timer may be inaccurate when used by multiple threads."
       << endl;
  cout << "MB, "
       << "IC, "
       << "OC, "
       << "IH, "
       << "IW, "
       << "G, "
       << "KH, "
       << "KW, "
       << "stride_h, "
       << "stride_w, "
       << "pad_h, "
       << "pad_w, "
       << "Type, "
       << "M, "
       << "N, "
       << "K, "
       << "Im2Col (ms), "
       << "Packing (ms), "
       << "Kernel (ms), "
       << "Postprocessing (ms), "
       << "fbgemmPacked (ms), "
       << "Total (ms), "
       << "GOPS" << endl;
#else
  cout << setw(8) << "MB, "
       << "IC, "
       << "OC, "
       << "IH, "
       << "IW, "
       << "G, "
       << "KH, "
       << "KW, "
       << "stride_h, "
       << "stride_w, "
       << "pad_h, "
       << "pad_w, "
       << "Type, "
       << "M, "
       << "N, "
       << "K, " << setw(5) << "GOPS" << endl;
#endif

  chrono::time_point<chrono::high_resolution_clock> begin, end;
  for (auto conv_p : shapes) {
    if (conv_p.IC % conv_p.G != 0 || conv_p.OC % conv_p.G != 0) {
      continue;
    }
    aligned_vector<uint8_t> Aint8(
        conv_p.MB * conv_p.IN_DIM[0] * conv_p.IN_DIM[1] * conv_p.IC);

    aligned_vector<uint8_t> Aint8_out(
        conv_p.MB * conv_p.OUT_DIM[0] * conv_p.OUT_DIM[1] * conv_p.K[0] *
        conv_p.K[1] * conv_p.IC);

    aligned_vector<int8_t> Bint8(
        conv_p.K[0] * conv_p.K[1] * conv_p.IC * conv_p.OC);

    aligned_vector<int32_t> Cint32_ref(
        conv_p.MB * conv_p.OUT_DIM[0] * conv_p.OUT_DIM[1] * conv_p.OC);
    //aligned_vector<int32_t> Cint32_fb(Cint32_ref.size());
    //increased buffer size for encoded row
    aligned_vector<int32_t> Cint32_fb_buffer(Cint32_ref.size() + conv_p.OC);
    //aligned_vector<int32_t> Cint32_fb2(Cint32_ref.size());
    aligned_vector<int32_t> Cint32_fb2_buffer(Cint32_ref.size() + conv_p.OC);
    aligned_vector<uint8_t> Cint8_ref(Cint32_ref.size());
    aligned_vector<uint8_t> Cint8_fb(Cint32_ref.size());
    aligned_vector<uint8_t> Cint8_fb2(Cint32_ref.size());

    // A matrix (input activations)
    randFill<uint8_t>(Aint8, 0, 5);
    //randFill<uint8_t>(Aint8, 1, 1);
    int32_t Aint8_zero_point = 4;

    // B matrix (weights)
    randFill<int8_t>(Bint8, -4, 4);
    //randFill<int8_t>(Bint8, 1, 1);
    aligned_vector<int32_t> Bint8_zero_point(1);
    randFill(Bint8_zero_point, -3, -1);
    aligned_vector<float> Bfp32(Bint8.begin(), Bint8.end());

    aligned_vector<float> C_multiplier(Bint8_zero_point.size());
    randFill(C_multiplier, 0.1234f / 2, 0.1234f * 3 / 2);
    int32_t C_zero_point = 5;

    int ncols_per_quant_group = conv_p.OC;
    // matrix dimensions after im2col
    int MDim = conv_p.MB * conv_p.OUT_DIM[0] * conv_p.OUT_DIM[1];
    int NDim = conv_p.OC / conv_p.G;
    int KDim = conv_p.K[0] * conv_p.K[1] * conv_p.IC;
    int KDimPerGroup = KDim / conv_p.G;

    // computing row offset
      vector<int32_t> row_offsets(MDim);
      vector<uint8_t> Aint8_im2col(MDim * KDim);
      im2col_ref(conv_p, Aint8.data(), Aint8_zero_point, Aint8_im2col.data());

      // computing column offset
      vector<int32_t> col_offsets(conv_p.G * NDim);
      for (int g = 0; g < conv_p.G; ++g) {
        col_offsets_with_zero_pt_s8acc32_ref(
            KDimPerGroup,
            NDim,
            NDim,
            Bint8.data() + g * KDimPerGroup * NDim,
            Bint8_zero_point.data() + g * NDim / ncols_per_quant_group,
            col_offsets.data() + g * NDim,
            ncols_per_quant_group);
      }

    // reference implementation
    conv_ref(
        conv_p,
        Aint8.data(),
        Aint8_zero_point,
        Bint8.data(),
        Cint32_ref.data());

      for (int g = 0; g < conv_p.G; ++g) {
        row_offsets_u8acc32_ref(
            MDim,
            KDimPerGroup,
            KDim,
            Aint8_im2col.data() + g * KDimPerGroup,
            row_offsets.data());

        requantize_u8acc32_ref(
            MDim,
            NDim,
            conv_p.G * NDim,
            Cint32_ref.data() + g * NDim,
            Cint8_ref.data() + g * NDim,
            C_multiplier.data() + g * NDim / ncols_per_quant_group,
            C_zero_point,
            Aint8_zero_point,
            Bint8_zero_point.data() + g * NDim / ncols_per_quant_group,
            row_offsets.data(),
            col_offsets.data() + g * NDim,
            nullptr,
            ncols_per_quant_group);
      }

    //cout << "out 0, 1: " << conv_p.OUT_DIM[0] << ", " << conv_p.OUT_DIM[1] << endl;
    //cout << "M, N, K: " << MDim << ", " << NDim << ", " << KDim << endl;

    // printMatrix(matrix_op_t::NoTranspose, Bint8.data(), KDim, NDim, NDim,
    // "B unpacked");
    // packedB.printPackedMatrix("B Packed");

    double nops = 2.0 * static_cast<double>(NITER) * MDim * NDim * KDim;
    double ttot = 0.0;
    string runType;

    vector<int32_t> row_offset_buf(
        PackEncodedAWithIm2Col<uint8_t, Acc_t>::rowOffsetBufferSize());

    //PackEncodedAWithIm2Col<uint8_t, Acc_t> packA(
    //    conv_p, Aint8.data(), nullptr, Aint8_zero_point, row_offset_buf.data());

    PackBMatrix<int8_t, Acc_t> packedB(
        matrix_op_t::NoTranspose,
        KDim,
        NDim,
        Bint8.data(),
        NDim,
        nullptr,
        conv_p.G);

    // no-op output process objects
    //DoNothing<int32_t, int32_t> doNothing32BitObj;
    //memCopy<> memcopyObj(doNothing32BitObj);

    runType = "FusedIm2Col";
    ttot = 0;
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
    double im2col_time = 0.0;
    double total_im2col_time = 0.0;
    double total_packing_time = 0.0;
    double total_computing_time = 0.0;
    double total_kernel_time = 0.0;
    double total_postprocessing_time = 0.0;
    double total_run_time = 0.0;
#endif
    for (auto i = 0; i < NWARMUP + NITER; ++i) {
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
      packing_time = 0.0;
      computing_time = 0.0;
      kernel_time = 0.0;
      postprocessing_time = 0.0;
      run_time = 0.0;
#endif
      llc_flush(llc);
      begin = chrono::high_resolution_clock::now();
      PackEncodedAWithIm2Col<uint8_t, Acc_t> packA(
        conv_p, Aint8.data(), nullptr, Aint8_zero_point, row_offset_buf.data());

      DoNothing<> doNothingObj{};
        ReQuantizeOutput<false, QuantizationGranularity::TENSOR> outputProcObj(
            doNothingObj,
            C_multiplier.data(),
            C_zero_point,
            Aint8_zero_point,
            Bint8_zero_point.data(),
            packA.getRowOffsetBuffer(),
            col_offsets.data(),
            nullptr,
            conv_p.G * NDim,
            conv_p.G);

      fbgemmPacked(
          packA,
          packedB,
          Cint8_fb.data(),
          Cint32_fb_buffer.data(),
          conv_p.G * NDim,
          outputProcObj,
          0,
          1);
      end = chrono::high_resolution_clock::now();

      if (i >= NWARMUP) {
        auto dur = chrono::duration_cast<chrono::nanoseconds>(end - begin);
        ttot += dur.count();
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
        total_packing_time += packing_time;
        total_computing_time += computing_time;
        total_kernel_time += kernel_time;
        total_postprocessing_time += postprocessing_time;
        total_run_time += run_time;
#endif
      }
    }

    cout << setw(4) << conv_p.MB << ", " << conv_p.IC << ", " << conv_p.OC
         << ", " << conv_p.IN_DIM[0] << ", " << conv_p.IN_DIM[1] << ", "
         << conv_p.G << ", " << conv_p.K[0] << ", " << conv_p.K[1] << ", "
         << conv_p.stride[0] << ", " << conv_p.stride[1] << ", "
         << conv_p.pad[0] << ", " << conv_p.pad[1] << ", ";

    cout << setw(13) << runType << ", " << setw(5) << fixed << setw(5)
         << setw(6) << MDim << ", " << setw(6) << NDim << ", " << setw(6)
         << KDim << ", ";
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
    cout << fixed << setprecision(6) << setw(8) << 0 << ", "
         << total_packing_time / (double)NITER / 1e6 << ", "
         << total_kernel_time / (double)NITER / 1e6 << ", "
         << total_postprocessing_time / (double)NITER / 1e6 << ", "
         << total_run_time / (double)NITER / 1e6 << ", "
         << ttot / (double)NITER / 1e6 << ", ";
#endif
    cout << setprecision(2) << nops / ttot << endl;
    cout << "fused time: " << (double)ttot/1e6 << std::endl;
    cout << "shape M, N, K: " << MDim << ", " << NDim << ", " << KDim << std::endl;

    compare_buffers(Cint8_ref.data(), Cint8_fb.data(), MDim, NDim, NDim, 5);

    runType = "UnfusedIm2Col";
    ttot = 0;
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
    total_im2col_time = 0.0;
    total_packing_time = 0.0;
    total_computing_time = 0.0;
    total_kernel_time = 0.0;
    total_postprocessing_time = 0.0;
    total_run_time = 0.0;
#endif
    for (auto i = 0; i < NWARMUP + NITER; ++i) {
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
      im2col_time = 0.0;
      packing_time = 0.0;
      computing_time = 0.0;
      kernel_time = 0.0;
      postprocessing_time = 0.0;
      run_time = 0.0;
#endif
      llc_flush(llc);
      begin = chrono::high_resolution_clock::now();

      im2col_ref(conv_p, Aint8.data(), Aint8_zero_point, Aint8_out.data());

#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
      end = chrono::high_resolution_clock::now();
      im2col_time =
          chrono::duration_cast<chrono::nanoseconds>(end - begin).count();
#endif

      // printMatrix(matrix_op_t::NoTranspose, Aint8_out.data(), MDim, KDim,
      // KDim, "A_out after im2col unpacked");

      PackAWithRowOffset<uint8_t, Acc_t> packAN(
          matrix_op_t::NoTranspose,
          MDim,
          KDim,
          Aint8_out.data(),
          KDim,
          nullptr,
          conv_p.G,
          row_offset_buf.data());

        DoNothing<> doNothingObj{};
        ReQuantizeOutput<false, QuantizationGranularity::TENSOR> outputProcObj(
            doNothingObj,
            C_multiplier.data(),
            C_zero_point,
            Aint8_zero_point,
            Bint8_zero_point.data(),
            packAN.getRowOffsetBuffer(),
            col_offsets.data(),
            nullptr,
            conv_p.G * NDim,
            conv_p.G);

      fbgemmPacked(
          packAN,
          packedB,
          Cint8_fb2.data(),
          Cint32_fb2_buffer.data(),
          conv_p.G * NDim,
          outputProcObj,
          0,
          1);
      end = chrono::high_resolution_clock::now();

      if (i >= NWARMUP) {
        auto dur = chrono::duration_cast<chrono::nanoseconds>(end - begin);
        ttot += dur.count();
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
        total_im2col_time += im2col_time;
        total_packing_time += packing_time;
        total_computing_time += computing_time;
        total_kernel_time += kernel_time;
        total_postprocessing_time += postprocessing_time;
        total_run_time += run_time;
#endif
      }
    }

    ((volatile char*)(llc.data()));

    // packedB.printPackedMatrix("bench B Packed");
    // printMatrix(matrix_op_t::NoTranspose, Cint32_fb.data(), MDim, NDim, NDim,
    // "C fb fp32");
    // printMatrix(matrix_op_t::NoTranspose, Cint32_fb2.data(),
    // MDim, NDim, NDim, "C fb2 fp32");
    // printMatrix(matrix_op_t::NoTranspose,
    // Cint32_ref.data(), MDim, NDim, NDim, "C ref fp32");

    cout << setw(4) << conv_p.MB << ", " << conv_p.IC << ", " << conv_p.OC
         << ", " << conv_p.IN_DIM[0] << ", " << conv_p.IN_DIM[1] << ", "
         << conv_p.G << ", " << conv_p.K[0] << ", " << conv_p.K[1] << ", "
         << conv_p.stride[0] << ", " << conv_p.stride[1] << ", "
         << conv_p.pad[0] << ", " << conv_p.pad[1] << ", ";

    cout << setw(13) << runType << ", " << setw(5) << fixed << setw(5)
         << setw(6) << MDim << ", " << setw(6) << NDim << ", " << setw(6)
         << KDim << ", ";
#ifdef FBGEMM_MEASURE_TIME_BREAKDOWN
    cout << fixed << setprecision(6) << setw(8)
         << total_im2col_time / (double)NITER / 1e6 << ", "
         << total_packing_time / (double)NITER / 1e6 << ", "
         << total_kernel_time / (double)NITER / 1e6 << ", "
         << total_postprocessing_time / (double)NITER / 1e6 << ", "
         << total_run_time / (double)NITER / 1e6 << ", "
         << ttot / (double)NITER / 1e6 << ", ";
#endif
    cout << setprecision(2) << nops / ttot << endl;
    cout << "unfused time: " << (double)ttot/1e6 << std::endl;

    compare_buffers(Cint8_ref.data(), Cint8_fb2.data(), MDim, NDim, NDim, 5);
  } // shapes
}

int main() {
#ifdef _OPENMP
  // Use 1 thread unless OMP_NUM_THREADS is explicit set.
  const char* val = getenv("OMP_NUM_THREADS");
  if (val == nullptr || !*val) {
    omp_set_num_threads(1);
  }
#endif
  //performance_test<int16_t>();
  performance_test<int32_t>();
  return 0;
}
