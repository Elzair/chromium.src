// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_metric.h"

#include "base/logging.h"

namespace rappor {

RapporMetric::RapporMetric(const std::string& metric_name,
                           const RapporParameters& parameters,
                           int32_t cohort)
    : metric_name_(metric_name),
      parameters_(parameters),
      bloom_filter_(parameters.bloom_filter_size_bytes,
                    parameters.bloom_filter_hash_function_count,
                    cohort * parameters.bloom_filter_hash_function_count) {
  DCHECK_GE(cohort, 0);
}

RapporMetric::~RapporMetric() {}

void RapporMetric::AddSample(const std::string& str) {
  bloom_filter_.AddString(str);
}

ByteVector RapporMetric::GetReport(const std::string& secret) const {
  // Generate a deterministically random mask of fake data using the
  // client's secret key + real data as a seed.  The inclusion of the secret
  // in the seed avoids correlations between real and fake data.
  // The seed isn't a human-readable string.
  const std::string personalization_string = metric_name_ +
      std::string(bytes().begin(), bytes().end());
  HmacByteVectorGenerator hmac_generator(bytes().size(), secret,
                                         personalization_string);
  const ByteVector fake_mask =
      hmac_generator.GetWeightedRandomByteVector(parameters().fake_prob);
  ByteVector fake_bits =
      hmac_generator.GetWeightedRandomByteVector(parameters().fake_one_prob);

  // Redact most of the real data by replacing it with the fake data, hiding
  // and limiting the amount of information an individual client reports on.
  const ByteVector* fake_and_redacted_bits =
      ByteVectorMerge(fake_mask, bytes(), &fake_bits);

  // Generate biased coin flips for each bit.
  ByteVectorGenerator coin_generator(bytes().size());
  const ByteVector zero_coins =
      coin_generator.GetWeightedRandomByteVector(parameters().zero_coin_prob);
  ByteVector one_coins =
      coin_generator.GetWeightedRandomByteVector(parameters().one_coin_prob);

  // Create a randomized response report on the fake and redacted data, sending
  // the outcome of flipping a zero coin for the zero bits in that data, and of
  // flipping a one coin for the one bits in that data, as the final report.
  return *ByteVectorMerge(*fake_and_redacted_bits, zero_coins, &one_coins);
}

}  // namespace rappor
