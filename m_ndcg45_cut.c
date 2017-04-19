/*
 Copyright (c) 2008 - Chris Buckley.
 Copyright (c) 2017 - Daniel Valcarce-

 Permission is granted for use and modification of this file for
 research, non-commercial purposes.
 */
#include "common.h"
#include "sysfunc.h"
#include "trec_eval.h"
#include "functions.h"
#include "trec_format.h"
double log2(double x);

static int te_calc_ndcg45_cut(const EPI *epi, const REL_INFO *rel_info,
		const RESULTS *results, const TREC_MEAS *tm, TREC_EVAL *eval);
static long long_cutoff_array[] = { 5, 10, 15, 20, 30, 100 };
static PARAMS default_ndcg45_cutoffs = {
NULL, sizeof(long_cutoff_array) / sizeof(long_cutoff_array[0]),
		&long_cutoff_array[0] };
static double get_gain(const long rel_level);

/* See trec_eval.h for definition of TREC_MEAS */
TREC_MEAS te_meas_ndcg45_cut =
		{ "ndcg45_cut",
				"    Normalized Discounted Cumulative Gain at cutoffs.\n\
    Compute a nDCG measure according.\n\
    Gain values are 1 for relevance value 4 and 2 for relevance\n\
	value 5 in the qrels file.\n\
    Cutoffs must be positive without duplicates\n\
    Default params: -m ndcg45_cut.5,10,15,20,30,100\n\
    Based on an implementation by Ian Soboroff\n",
				te_init_meas_a_float_cut_long, te_calc_ndcg45_cut,
				te_acc_meas_a_cut, te_calc_avg_meas_a_cut,
				te_print_single_meas_a_cut, te_print_final_meas_a_cut,
				(void *) &default_ndcg45_cutoffs, -1 };

static int te_calc_ndcg45_cut(const EPI *epi, const REL_INFO *rel_info,
		const RESULTS *results, const TREC_MEAS *tm, TREC_EVAL *eval) {
	long *cutoffs = (long *) tm->meas_params->param_values;
	long cutoff_index = 0;
	RES_RELS res_rels;
	double gain, sum;
	double ideal_dcg; /* ideal discounted cumulative gain */
	long cur_lvl, lvl_count;
	long i;

	if (UNDEF == te_form_res_rels(epi, rel_info, results, &res_rels))
		return (UNDEF);

	sum = 0.0;
	for (i = 0; i < res_rels.num_ret; i++) {
		if (i == cutoffs[cutoff_index]) {
			/* Calculate previous cutoff threshold.
			 Note i guaranteed to be positive by init_meas */
			eval->values[tm->eval_index + cutoff_index].value = sum;
			if (++cutoff_index == tm->meas_params->num_params)
				break;
			if (epi->debug_level > 0)
				printf("ndcg45_cut: cutoff %ld dcg %6.4f\n", i, sum);
		}
		gain = get_gain(res_rels.results_rel_list[i]);
		if (gain > 0) {
			/* Note: i+2 since doc i has rank i+1 */
			sum += gain / log2((double) (i + 2));
			if (epi->debug_level > 1)
				printf("ndcg45_cut: %ld %3.1f %6.4f\n", i, gain, sum);
		}
	}
	/* calculate values for those cutoffs not achieved */
	while (cutoff_index < tm->meas_params->num_params) {
		eval->values[tm->eval_index + cutoff_index].value = sum;
		if (epi->debug_level > 0)
			printf("ndcg45_cut: cutoff %ld dcg %6.4f\n", cutoffs[cutoff_index],
					sum);
		cutoff_index++;
	}
	/* Calculate ideal discounted cumulative gain for this topic, and
	 normalize previous sum by it */
	cutoff_index = 0;
	cur_lvl = res_rels.num_rel_levels - 1;
	lvl_count = 0;
	ideal_dcg = 0.0;
	for (i = 0; 1; i++) {
		lvl_count++;
		while (cur_lvl > 0 && lvl_count > res_rels.rel_levels[cur_lvl]) {
			cur_lvl--;
			lvl_count = 1;
		}
		if (cur_lvl == 0) {
			break;
		}

		if (i == cutoffs[cutoff_index]) {
			/* Calculate previous cutoff threshold.
			 Note i guaranteed to be positive by init_meas */
			if (ideal_dcg > 0.0)
				eval->values[tm->eval_index + cutoff_index].value /= ideal_dcg;
			if (epi->debug_level > 0)
				printf("ndcg45_cut: cutoff %ld idcg %6.4f\n", i, ideal_dcg);
			if (++cutoff_index == tm->meas_params->num_params)
				break;
		}
		if ((gain = get_gain(cur_lvl)) == 0.0) {
			break;
		}
		ideal_dcg += gain / (double) log2((double) (i + 2));
		if (epi->debug_level > 0)
			printf("ndcg45_cut:%ld %3.1f %6.4f\n", i, gain, ideal_dcg);
	}

	/* calculate values for those cutoffs not achieved */
	while (cutoff_index < tm->meas_params->num_params) {
		if (ideal_dcg > 0.0)
			eval->values[tm->eval_index + cutoff_index].value /= ideal_dcg;
		if (epi->debug_level > 0) {
			printf("ndcg45_cut: cutoff %ld idcg %6.4f\n", cutoffs[cutoff_index],
					ideal_dcg);
		}
		cutoff_index++;
	}

	return (1);
}

static inline double get_gain(const long rel_level) {
	switch (rel_level) {
	case 4:
		return 1.0;
	case 5:
		return 2.0;
	default:
		return 0.0;
	}
}
