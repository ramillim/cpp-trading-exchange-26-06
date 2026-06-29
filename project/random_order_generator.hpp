#pragma once

#include <random>
#include <cstdint>

class RandomOrderDataGenerator {
public:
    uint64_t generate_uuid_v4_as_uint64() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        return dis(gen);
    }

    uint64_t get_next_order_id() {
        static uint64_t current_id = 1;
        return current_id++;
    }

    uint32_t generate_random_size() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint32_t> dis(1, 100);
        static std::bernoulli_distribution fractional_dis(0.1);  // 10% probability

        if (fractional_dis(gen)) {
            // Generate a fractional amount like 250, 500, 750
            static std::uniform_int_distribution<uint32_t> frac_val_dis(1, 3);
            return frac_val_dis(gen) * 250;
        }

        return dis(gen) * 1000;
    }

    uint64_t generate_random_price() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static uint64_t last_price = 15000;

        static std::bernoulli_distribution cluster_dis(0.75);
        static std::uniform_int_distribution<int64_t> offset_dis(-300, 300);
        static std::uniform_int_distribution<uint64_t> wide_dis(10000, 20000);
        static std::bernoulli_distribution round_dis(0.9);  // 90% chance to be multiple of 100

        if (cluster_dis(gen)) {
            int64_t offset = offset_dis(gen);
            if (offset < 0 && static_cast<uint64_t>(-offset) >= last_price) {
                last_price = 100;  // Minimum price
            } else {
                last_price += offset;
            }
        } else {
            last_price = wide_dis(gen);
        }

        if (round_dis(gen)) {
            last_price = ((last_price + 50) / 100) * 100;
        }

        if (last_price < 100) last_price = 100;

        return last_price;
    }
};
