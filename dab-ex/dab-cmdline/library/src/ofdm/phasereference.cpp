#
/*
 *    Copyright (C) 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the DAB-library
 *
 *    DAB-library is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    DAB-library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with DAB-library; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	"phasereference.h" 
#include	"string.h"
#include	"dab-params.h"
#include <vector> 	// Include the vector header
#include <iostream>
#include <string>
#include <sstream> // include the stringstream header
#include 	"channel.h"

void    showCorrelation (int32_t dots, int marker, vector<int> &v) {
std::stringstream ss;

        if (v. size () > 0) {
           ss << "** DBG: Best matches at ";
           for (uint8_t i = 0; i < v. size (); i ++)
		ss << v.at(i) << " ";
        }
	cerr << ss.str() << endl;
//	fprintf(stderr, "DBG: %fHz\n",estimate_FrequencyOffset(v));
}

/**
  *	\class phaseReference
  *	Implements the correlation that is used to identify
  *	the "first" element (following the cyclic prefix) of
  *	the first non-null block of a frame
  *	The class inherits from the phaseTable.
  */
#define	PILOTS	100
#define	TAPS	100
	phaseReference::phaseReference (uint8_t	dabMode,
	                                int16_t	diff_length):
	                                     phaseTable (dabMode),
	                                     params (dabMode),
	                                     my_fftHandler (dabMode) {
int32_t	i;
float	Phi_k;
        this    -> T_u          = params. get_T_u ();
        this    -> T_g          = params. get_T_g ();
        this    -> diff_length  = diff_length;
        refTable.               resize (T_u);
        phaseDifferences.       resize (diff_length);
        fft_buffer              = my_fftHandler. getVector ();

        for (i = 0; i < T_u; i ++)
	   refTable [i] = std::complex<float> (0, 0);

        for (i = 1; i <= params. get_carriers () / 2; i ++) {
           Phi_k =  get_Phi (i);
           refTable [i] = std::complex<float> (cos (Phi_k), sin (Phi_k));
           Phi_k = get_Phi (-i);
           refTable [T_u - i] = std::complex<float> (cos (Phi_k), sin (Phi_k));
        }
//
//      prepare a table for the coarse frequency synchronization
	shiftFactor	= this -> diff_length / 4;
	shiftFactor =0;
//	diff_length=128;
//	fprintf(stderr,"** DBG DIFF=%d\n",diff_length);
        for (i = 0; i < diff_length; i ++) { // (i = 0; i < diff_length; i ++) {
           phaseDifferences [i] = // [i]
	                abs (arg (refTable [(T_u + i) % T_u] *
                                  conj (refTable [(T_u + i + 1) % T_u])));
	   phaseDifferences [i] *= phaseDifferences [i];
	} 
	
/*        if (phaseDifferences. size () > 0) {
           fprintf(stderr,"** DBG: PD=");
           for (uint8_t i = 0; i < phaseDifferences. size (); i ++) {
		fprintf(stderr, "%f ",phaseDifferences.at(i));
	   }
        } fprintf(stderr,"\n"); */
	
//	TO BE DONE BY Jan van Katwijk ... https://www.diva-portal.org/smash/get/diva2:406302/FULLTEXT01.pdf
//	theEstimator    = new channel (refTable, PILOTS, TAPS);
}

	phaseReference::~phaseReference (void) {
	delete theEstimator;
}

/**
  *	\brief findIndex
  *	the vector v contains "T_u" samples that are believed to
  *	belong to the first non-null block of a DAB frame.
  *	We correlate the data in this vector with the predefined
  *	data, and if the maximum exceeds a threshold value,
  *	we believe that that indicates the first sample we were
  *	looking for.
  */
int32_t	phaseReference::findIndex (std::complex<float> *v, int threshold) {
int32_t	i;
int32_t	maxIndex	= -1;
float	sum		= 0;
float	Max		= -1000; // -10000
float	lbuf [T_u / 2];

	memcpy (fft_buffer, v, T_u * sizeof (std::complex<float>));
	my_fftHandler. do_FFT ();

//	into the frequency domain, now correlate
	for (i = 0; i < T_u; i ++) 
	   fft_buffer [i] *= conj (refTable [i]);
//	and, again, back into the time domain
	my_fftHandler. do_iFFT ();
/**
  *	We compute the average signal value ...
  */
	for (i = 0; i < T_u / 2; i ++) {
	   lbuf [i] = jan_abs (fft_buffer [i]); // abs( fft_buffer [i]);
	   sum += lbuf [i];
	}

	sum /= T_u / 2; 
/* Old 2017 algorithm 
	for (i = 0; i < 50; i ++) {
	   float absValue = abs (fft_buffer [T_g - 40 + i]);
	   if (absValue > Max) {
	      maxIndex = T_g - 40 + i;
	      Max = absValue;
	   }
	} */
/* New one */
	vector<int> indices;	// requires vector class!
	for (i = T_g - 250; i < T_g + 250; i ++) {
	   if (lbuf [i] / sum > threshold)  {
	      bool foundOne = true;
	      for (int j = 1; (j < 10) && (i + j < T_g + 250); j ++) {
	         if (lbuf [i + j] > lbuf [i]) {
	            foundOne = false;
	            break;
	         }
	      }
	      if (foundOne) {
	         indices. push_back (i);
	         if (lbuf [i]> Max){
	            Max = lbuf [i];
	            maxIndex = i;
	         }
	         i += 10;
	      }
	   }
	}

/**
  *	that gives us a basis for validating the result
  */
	if (Max / sum < threshold) { // < threshold * sum) {
//	   fprintf(stderr, "** DBG: maxIdx=%f < %f\n",- abs (Max / sum) -1,threshold * sum);
	   return  - abs (Max / sum) - 1;
	}
	else {
//	   fprintf(stderr, "** DBG: maxIdx=%d\n",maxIndex);
//	   showCorrelation (T_u / 2, T_g, indices);
	   return maxIndex;	
	}
}

#define SEARCH_RANGE    (2 * 35)
int16_t phaseReference::estimateOffset (std::complex<float> *v) {
int16_t i, j, index_1 = 100, index_2 = 100;
#ifdef _MSC_VER
float   *computedDiffs = (float *)_alloca((SEARCH_RANGE + diff_length + 1) * sizeof(float));
#else
float   computedDiffs [SEARCH_RANGE + diff_length + 1];
#endif

	for (i = 0; i < T_u; i ++)
	   fft_buffer [i] = v [i];

	my_fftHandler. do_FFT ();

	for (i = T_u - SEARCH_RANGE / 2;
	     i < T_u + SEARCH_RANGE / 2 + diff_length; i ++) 
	   computedDiffs [i - (T_u - SEARCH_RANGE / 2)] =
	      arg (fft_buffer [(i - shiftFactor) % T_u] *
	           conj (fft_buffer [(i - shiftFactor + 1) % T_u]));

	for (i = 0; i < SEARCH_RANGE + diff_length; i ++)
	   computedDiffs [i] *= computedDiffs [i];

        float   Mmin = 10000;
        float   Mmax = 0;

	for (i = T_u - SEARCH_RANGE / 2;
             i < T_u + SEARCH_RANGE / 2; i ++) {
           int sum_1 = 0;
           int sum_2 = 0;
           for (j = 0; j < diff_length; j ++) {
              if (phaseDifferences [j] < 0.05)
                 sum_1 += computedDiffs [i - (T_u - SEARCH_RANGE / 2) + j];
	      if (phaseDifferences [j] > M_PI - 0.05)
                 sum_2 += computedDiffs [i - (T_u - SEARCH_RANGE / 2) + j];
           }
           if (sum_1 < Mmin) {
              Mmin = sum_1;
              index_1 = i;
           }
           if (sum_2 > Mmax) {
              Mmax = sum_2;
              index_2 = i;
           }
        }

        return index_1 == index_2 ? index_1 - T_u : 100;
}

//int32_t	phaseReference::findIndex       (std::complex<float> *v, int threshold) {
//int16_t phaseReference::estimateOffset  (std::complex<float> *v) {

void    phaseReference::estimate        (std::complex<float> *v) {
std::complex<float> h_td [TAPS];

        for (int i = 0; i < T_u; i ++)
           fft_buffer [i] = v [i];
 
        my_fftHandler. do_FFT();
//      theEstimator -> estimate (fft_buffer, h_td);
//      float   Tau             = 0;
//      float   teller          = 0;
//      float   noemer          = 0;
//      for (int i = - TAPS / 2; i < TAPS / 2; i ++) {
//         float h_ts = abs (h_td [TAPS / 2 + i]) * abs (h_td [TAPS / 2 + i]);
//         teller += i * h_ts;
//         noemer += h_ts;
//      }
//      Tau     = teller / noemer;
//      teller  = 0;
//      noemer  = 0;
//
//      for (int i = -TAPS / 2; i < TAPS / 2; i ++) {
//         float h_ts = abs (h_td [TAPS / 2 + i]) * abs (h_td [TAPS / 2 + i]);
//         teller += (i - Tau) * (i - Tau) * h_ts;
//         noemer += h_ts;
//      }
//      
//      fprintf (stderr, "Tau = %f, rms delay spread %f\n", Tau,
//                                                      teller / noemer);


}

//      An alternative way to compute the small frequency offset
//      is to look at the phase offset of subsequent carriers
//      in block 0, compared to the values as computed from the
//      reference block.
//      The values are reasonably close to the values computed
//      on the fly
/*#define LLENGTH 100
float   phaseReference::estimate_FrequencyOffset (std::complex<float> *v) {
int16_t i;
float pd        = 0;

        for (i = - LLENGTH / 2 ; i < LLENGTH / 2; i ++) {
           std::complex<float> a1 = refTable [(T_u + i) % T_u] * conj (refTable [(T_u + i + 1) % T_u]
           std::complex<float> a2 = fft_buffer [(T_u + i) % T_u] * conj (fft_buffer [(T_u + i + 1) %
           pd += arg (a2) - arg (a1);
        }
        return pd / LLENGTH;
}
*/