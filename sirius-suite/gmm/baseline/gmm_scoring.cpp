#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <string>

#include "../../utils/timer.h"

using namespace std;

float feature_vect[] = {2.240018,    2.2570236,    0.11304555,   -0.21307051,
                        0.8988138,   0.039065503,  0.023874786,  0.13153112,
                        0.15324382,  0.16986738,   -0.020297153, -0.26773554,
                        0.40202165,  0.35923952,   0.060746543,  0.35402644,
                        0.086052455, -0.10499257,  0.04395058,   0.026407119,
                        -0.48301497, 0.120889395,  0.67980754,   -0.19875681,
                        -0.5443737,  -0.039534688, 0.20888293,   0.054865785,
                        -0.4846478,  0.1,          0.1,          0.1};

float *means_vect;
float *precs_vect;
float *weight_vect;
float *factor_vect;
float *score_vect;

float logZero = -3.4028235E38;
float logBase = 1.0001;
float maxLogValue = 7097004.5;
float minLogValue = -7443538.0;
float naturalLogBase = (float)1.00011595E-4;
float inverseNaturalLogBase = 9998.841;

int comp_size = 32;
int feat_size = 29;
int senone_size = 5120;

void computeScore_seq(float *feature_vect, float *means_vect, float *precs_vect,
                      float *weight_vect, float *factor_vect) {
  int comp_size = 32;
  int feat_size = 29;
  int senone_size = 5120;

  for (int i = 0; i < senone_size; i++) {
    score_vect[i] = logZero;

    for (int j = 0; j < comp_size; j++) {
      float logDval = 0.0f;

      for (int k = 0; k < feat_size; k++) {
        int idx = k + comp_size * j + i * comp_size * comp_size;
        float logDiff = feature_vect[k] - means_vect[idx];
        logDval += logDiff * logDiff * precs_vect[idx];
      }

      // Convert to the appropriate base.
      if (logDval != logZero) {
        logDval = logDval * inverseNaturalLogBase;
      }

      int idx2 = j + i * comp_size;
      logDval -= factor_vect[idx2];

      if (logDval < logZero) {
        logDval = logZero;
      }

      float logVal2 = logDval + weight_vect[idx2];

      float logHighestValue = score_vect[i];
      float logDifference = score_vect[i] - logVal2;

      // difference is always a positive number
      if (logDifference < 0) {
        logHighestValue = logVal2;
        logDifference = -logDifference;
      }

      float logValue = -logDifference;
      float logInnerSummation;
      if (logValue < minLogValue) {
        logInnerSummation = 0.0;
      } else if (logValue > maxLogValue) {
        logInnerSummation = FLT_MAX;

      } else {
        if (logValue == logZero) {
          logValue = logZero;
        } else {
          logValue = logValue * naturalLogBase;
        }
        logInnerSummation = exp(logValue);
      }

      logInnerSummation += 1.0;

      float returnLogValue;
      if (logInnerSummation <= 0.0) {
        returnLogValue = logZero;

      } else {
        returnLogValue =
            (float)(log(logInnerSummation) * inverseNaturalLogBase);
        if (returnLogValue > FLT_MAX) {
          returnLogValue = FLT_MAX;
        } else if (returnLogValue < -FLT_MAX) {
          returnLogValue = -FLT_MAX;
        }
      }
      // sum log
      score_vect[i] = logHighestValue + returnLogValue;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "[ERROR] Input file required.\n\n");
    fprintf(stderr, "Usage: %s [INPUT FILE]\n\n", argv[0]);
    exit(0);
  }

  STATS_INIT("kernel", "gaussian_mixture_model");
  PRINT_STAT_STRING("abrv", "gmm");

  int means_array_size = senone_size * comp_size * comp_size;
  int comp_array_size = senone_size * comp_size;

  means_vect = (float *)malloc(means_array_size * sizeof(float));
  precs_vect = (float *)malloc(means_array_size * sizeof(float));
  weight_vect = (float *)malloc(comp_array_size * sizeof(float));
  factor_vect = (float *)malloc(comp_array_size * sizeof(float));

  score_vect = (float *)malloc(senone_size * sizeof(float));

  // load model from file
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    printf("Can’t open file\n");
    exit(-1);
  }

  int idx = 0;
  for (int i = 0; i < senone_size; i++) {
    for (int j = 0; j < comp_size; j++) {
      for (int k = 0; k < comp_size; k++) {
        float elem;
        if (!fscanf(fp, "%f", &elem)) break;
        means_vect[idx] = elem;
        ++idx;
      }
    }
  }

  idx = 0;
  for (int i = 0; i < senone_size; i++) {
    for (int j = 0; j < comp_size; j++) {
      for (int k = 0; k < comp_size; k++) {
        float elem;
        if (!fscanf(fp, "%f", &elem)) break;
        precs_vect[idx] = elem;
        idx = idx + 1;
      }
    }
  }

  idx = 0;
  for (int i = 0; i < senone_size; i++) {
    for (int j = 0; j < comp_size; j++) {
      float elem;
      if (!fscanf(fp, "%f", &elem)) break;
      weight_vect[idx] = elem;
      idx = idx + 1;
    }
  }

  idx = 0;
  for (int i = 0; i < senone_size; i++) {
    for (int j = 0; j < comp_size; j++) {
      float elem;
      if (!fscanf(fp, "%f", &elem)) break;
      factor_vect[idx] = elem;
      idx = idx + 1;
    }
  }

  fclose(fp);

  tic();
  computeScore_seq(feature_vect, means_vect, precs_vect, weight_vect,
                   factor_vect);
  PRINT_STAT_DOUBLE("gmm", toc());

  STATS_END();

// write for correctness check
#if TESTING
  FILE *f = fopen("../input/gmm_scoring.baseline", "w");

  for (int i = 0; i < senone_size; ++i) fprintf(f, "%.0f\n", score_vect[i]);

  fclose(f);
#endif

  /* Clean up and exit */
  free(means_vect);
  free(precs_vect);

  free(weight_vect);
  free(factor_vect);

  free(score_vect);

  return 0;
}
