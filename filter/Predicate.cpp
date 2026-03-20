#include "Predicate.h"
#include <arm_neon.h>
#include <iostream>

void Predicate::lineitem_filter_indices(const std::shared_ptr<arrow::Table>& rg, const int64_t* raw_partkey, const int64_t* raw_qty, int64_t count, const std::unordered_map<int64_t, uint8_t>& hash_table, std::vector<int64_t>& matching_indices) {
    //int64_t total_int = 0;
    matching_indices.clear();
    // --- dictionary lookup for shipinstruct ---
    auto col_instruct = rg->column(0);
    auto dict_instruct = arrow::internal::checked_cast<arrow::DictionaryArray*>(col_instruct->chunk(0).get());
    auto str_instruct  = arrow::internal::checked_cast<arrow::StringArray*>(dict_instruct->dictionary().get());
    int32_t deliver_idx = -1;
    for(int32_t i = 0; i < str_instruct->length(); ++i)
        if(str_instruct->GetString(i) == "DELIVER IN PERSON") { deliver_idx = i; break; }

    // --- dictionary lookup for shipmode ---
    auto col_mode = rg->column(1);
    auto dict_mode = arrow::internal::checked_cast<arrow::DictionaryArray*>(col_mode->chunk(0).get());
    auto str_mode  = arrow::internal::checked_cast<arrow::StringArray*>(dict_mode->dictionary().get());
    int32_t air_idx = -1;
    for(int32_t i = 0; i < str_mode->length(); ++i) {
        auto s = str_mode->GetString(i);
        if(s == "AIR") {
            air_idx   = i;
            break;
        }
    }

    const int32_t* raw_instruct = arrow::internal::checked_cast<arrow::Int32Array*>(dict_instruct->indices().get())->raw_values();
    const int32_t* raw_mode     = arrow::internal::checked_cast<arrow::Int32Array*>(dict_mode->indices().get())->raw_values();

    // --- SIMD targets ---
    const int8x16_t vdeliver = vdupq_n_s8((int8_t)deliver_idx);
    const int8x16_t vair     = vdupq_n_s8((int8_t)air_idx);

    int64_t i = 0;

    for(; i + 16 <= count; i += 16) {
        // narrow instruct int32 -> int8
        int8x16_t ni = vcombine_s8(
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_instruct+i)),   vmovn_s32(vld1q_s32(raw_instruct+i+4)))),
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_instruct+i+8)), vmovn_s32(vld1q_s32(raw_instruct+i+12))))
        );

        // narrow mode int32 -> int8
        int8x16_t nm = vcombine_s8(
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_mode+i)),   vmovn_s32(vld1q_s32(raw_mode+i+4)))),
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_mode+i+8)), vmovn_s32(vld1q_s32(raw_mode+i+12))))
        );

        // instruct = 'DELIVER IN PERSON'
        uint8x16_t instruct_match = vceqq_s8(ni, vdeliver);

        // mode = 'AIR'
        uint8x16_t mode_match = vceqq_s8(nm, vair); 

        // combine
        uint8x16_t pass = vandq_u8(instruct_match, mode_match);

        // extract lanes for hash probe + quantity check
        uint8_t lanes[16];
        vst1q_u8(lanes, pass);
        for(int l = 0; l < 16; ++l) {
            if(!lanes[l]) continue;
            int64_t row = i + l;

            // hash probe
            auto it = hash_table.find(raw_partkey[row]);
            if(it == hash_table.end()) continue;

            // quantity check per brand
            int64_t qty = raw_qty[row];
            uint8_t brand = it->second;
            bool match = (brand == 1 && qty >= 800  && qty <= 1800)
              || (brand == 2 && qty >= 1000 && qty <= 2000)
              || (brand == 3 && qty >= 2400  && qty <= 3400);

            if(match){
                // std::cerr << "partkey=" << raw_partkey[row]
                // << " price=" << raw_price[row]
                // << " discount=" << raw_discount[row]
                // << " revenue=" << (raw_price[row] * (100 - raw_discount[row]))
                // << std::endl;

                //total_int += raw_price[row] * (100 - raw_discount[row]);

                matching_indices.push_back(row);
            }
        }
    }

    // scalar tail
    for(; i < count; ++i) {
        if(raw_instruct[i] != deliver_idx) continue;
        if(raw_mode[i] != air_idx) continue;

        auto it = hash_table.find(raw_partkey[i]);
        if(it == hash_table.end()) continue;

        int64_t qty = raw_qty[i];
        uint8_t brand = it->second;
        bool match = (brand == 1 && qty >= 800  && qty <= 1800)
                      || (brand == 2 && qty >= 1000 && qty <= 2000)
                      || (brand == 3 && qty >= 2400  && qty <= 3400);

        if(match) {
            // std::cerr << "partkey=" << raw_partkey[i]
            //   << " price=" << raw_price[i]
            //   << " discount=" << raw_discount[i]
            //   << " revenue=" << (raw_price[i] * (100 - raw_discount[i]))
            //   << std::endl;

            //total_int += raw_price[i] * (10000 - raw_discount[i]);
            
            matching_indices.push_back(i);
        }
    }

    return;
}

void Predicate::part_filter(const std::shared_ptr<arrow::Table>& rg, std::unordered_map<int64_t, uint8_t>& hash_table) {
    
    auto brand_col = rg->column(1);
    auto brand_dict = arrow::internal::checked_cast<arrow::DictionaryArray*>(brand_col->chunk(0).get());
    auto brand_str  = arrow::internal::checked_cast<arrow::StringArray*>(brand_dict->dictionary().get());
    
    int32_t idx22 = -1, idx23 = -1, idx12 = -1;
    for(int32_t i = 0; i < brand_str->length(); ++i) {
        auto s = brand_str->GetString(i);
        if(s == "Brand#22") idx22 = i;
        else if(s == "Brand#23") idx23 = i;
        else if(s == "Brand#12") idx12 = i;
    }

    auto container_col = rg->column(3);
    auto container_dict = arrow::internal::checked_cast<arrow::DictionaryArray*>(container_col->chunk(0).get());
    auto container_str  = arrow::internal::checked_cast<arrow::StringArray*>(container_dict->dictionary().get());

    // SM containers (Brand#22)
    int32_t sm_case=-1, sm_box=-1, sm_pack=-1, sm_pkg=-1;
    // MED containers (Brand#23)
    int32_t med_bag=-1, med_box=-1, med_pkg=-1, med_pack=-1;
    // LG containers (Brand#12)
    int32_t lg_case=-1, lg_box=-1, lg_pack=-1, lg_pkg=-1;

    for(int32_t i = 0; i < container_str->length(); ++i) {
        auto s = container_str->GetString(i);
        if(s == "SM CASE")  sm_case  = i;
        else if(s == "SM BOX")   sm_box   = i;
        else if(s == "SM PACK")  sm_pack  = i;
        else if(s == "SM PKG")   sm_pkg   = i;
        else if(s == "MED BAG")  med_bag  = i;
        else if(s == "MED BOX")  med_box  = i;
        else if(s == "MED PKG")  med_pkg  = i;
        else if(s == "MED PACK") med_pack = i;
        else if(s == "LG CASE")  lg_case  = i;
        else if(s == "LG BOX")   lg_box   = i;
        else if(s == "LG PACK")  lg_pack  = i;
        else if(s == "LG PKG")   lg_pkg   = i;
    }

    // --- Raw arrays ---
    auto* brand_idx     = arrow::internal::checked_cast<arrow::Int32Array*>(brand_dict->indices().get());
    auto* container_idx = arrow::internal::checked_cast<arrow::Int32Array*>(container_dict->indices().get());
    auto* partkey_arr   = arrow::internal::checked_cast<arrow::Int64Array*>(rg->column(0)->chunk(0).get());
    auto* size_arr      = arrow::internal::checked_cast<arrow::Int32Array*>(rg->column(2)->chunk(0).get());

    const int32_t* raw_brand     = brand_idx->raw_values();
    const int32_t* raw_container = container_idx->raw_values();
    const int64_t* raw_partkey   = partkey_arr->raw_values();
    const int32_t* raw_size      = size_arr->raw_values();
    const int64_t  count         = brand_idx->length();

    // --- SIMD loop ---
    const int8x16_t vb22 = vdupq_n_s8((int8_t)idx22);
    const int8x16_t vb23 = vdupq_n_s8((int8_t)idx23);
    const int8x16_t vb12 = vdupq_n_s8((int8_t)idx12);

    const int8x16_t vsm_case  = vdupq_n_s8((int8_t)sm_case);
    const int8x16_t vsm_box   = vdupq_n_s8((int8_t)sm_box);
    const int8x16_t vsm_pack  = vdupq_n_s8((int8_t)sm_pack);
    const int8x16_t vsm_pkg   = vdupq_n_s8((int8_t)sm_pkg);

    const int8x16_t vmed_bag  = vdupq_n_s8((int8_t)med_bag);
    const int8x16_t vmed_box  = vdupq_n_s8((int8_t)med_box);
    const int8x16_t vmed_pkg  = vdupq_n_s8((int8_t)med_pkg);
    const int8x16_t vmed_pack = vdupq_n_s8((int8_t)med_pack);

    const int8x16_t vlg_case  = vdupq_n_s8((int8_t)lg_case);
    const int8x16_t vlg_box   = vdupq_n_s8((int8_t)lg_box);
    const int8x16_t vlg_pack  = vdupq_n_s8((int8_t)lg_pack);
    const int8x16_t vlg_pkg   = vdupq_n_s8((int8_t)lg_pkg);

    int64_t i = 0;
    for(; i + 16 <= count; i += 16) {
        // narrow brand int32 -> int8
        int8x16_t nb = vcombine_s8(
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_brand+i)),    vmovn_s32(vld1q_s32(raw_brand+i+4)))),
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_brand+i+8)),  vmovn_s32(vld1q_s32(raw_brand+i+12))))
        );

        // narrow container int32 -> int8
        int8x16_t nc = vcombine_s8(
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_container+i)),   vmovn_s32(vld1q_s32(raw_container+i+4)))),
            vmovn_s16(vcombine_s16(vmovn_s32(vld1q_s32(raw_container+i+8)), vmovn_s32(vld1q_s32(raw_container+i+12))))
        );

        // brand matches
        uint8x16_t match22 = vceqq_s8(nb, vb22);
        uint8x16_t match23 = vceqq_s8(nb, vb23);
        uint8x16_t match12 = vceqq_s8(nb, vb12);

        // container matches per brand group
        uint8x16_t sm_match  = vorrq_u8(vorrq_u8(vceqq_s8(nc, vsm_case),  vceqq_s8(nc, vsm_box)),
                                         vorrq_u8(vceqq_s8(nc, vsm_pack),  vceqq_s8(nc, vsm_pkg)));
        uint8x16_t med_match = vorrq_u8(vorrq_u8(vceqq_s8(nc, vmed_bag),  vceqq_s8(nc, vmed_box)),
                                         vorrq_u8(vceqq_s8(nc, vmed_pkg),  vceqq_s8(nc, vmed_pack)));
        uint8x16_t lg_match  = vorrq_u8(vorrq_u8(vceqq_s8(nc, vlg_case),  vceqq_s8(nc, vlg_box)),
                                         vorrq_u8(vceqq_s8(nc, vlg_pack),  vceqq_s8(nc, vlg_pkg)));

        // brand AND container
        uint8x16_t cand22 = vandq_u8(match22, sm_match);
        uint8x16_t cand23 = vandq_u8(match23, med_match);
        uint8x16_t cand12 = vandq_u8(match12, lg_match);

        // any candidate
        uint8x16_t any = vorrq_u8(vorrq_u8(cand22, cand23), cand12);

        // extract matching lanes for size check + hash table insert
        uint8_t lanes[16];
        vst1q_u8(lanes, any);
        for(int l = 0; l < 16; ++l) {
            if(!lanes[l]) continue;
            int64_t idx = i + l;
            int32_t size = raw_size[idx];

            // size check per brand
            uint8_t which = lanes[l]; // 0xFF
            bool ok = false;
            if(raw_brand[idx] == idx22 && size >= 1 && size <= 5)  ok = true;
            if(raw_brand[idx] == idx23 && size >= 1 && size <= 10) ok = true;
            if(raw_brand[idx] == idx12 && size >= 1 && size <= 15) ok = true;

            if(ok) {
                uint8_t brand_bits = (raw_brand[idx] == idx22) ? 1 :
                                     (raw_brand[idx] == idx23) ? 2 : 3;
                hash_table[raw_partkey[idx]] = brand_bits;
            }
        }
    }

    for(; i < count; ++i) {
        int32_t b = raw_brand[i];
        int32_t c = raw_container[i];
        int32_t s = raw_size[i];
        bool ok = false;
        if(b == idx22 && (c==sm_case||c==sm_box||c==sm_pack||c==sm_pkg) && s>=1 && s<=5)  ok=true;
        if(b == idx23 && (c==med_bag||c==med_box||c==med_pkg||c==med_pack) && s>=1 && s<=10) ok=true;
        if(b == idx12 && (c==lg_case||c==lg_box||c==lg_pack||c==lg_pkg) && s>=1 && s<=15) ok=true;
        if(ok) {
            uint8_t brand_bits = (b==idx22)?1:(b==idx23)?2:3;
            hash_table[raw_partkey[i]] = brand_bits;
        }
    }
}

bool Predicate::shouldScanRowGroup(const Table& table, size_t rg) {
    auto stats = table.getColumnStats(rg, 3); // example: l_shipmode

    std::cout << stats.col_name << '\n';

    // std::cout << "Min: ";
    // std::visit([](auto&& val){ std::cout << val; }, stats.min_val);
    // std::cout << "\n";

    // std::cout << "Max: ";
    // std::visit([](auto&& val){ std::cout << val; }, stats.max_val);
    // std::cout << "\n";

    //Check if the Row has bloom filter + if a AIR exist inside
    if (stats.has_bloom && stats.bloom_filter) {
        checkBloomFilter("Brand#12", stats.bloom_filter);
    }

    return true;
}

bool Predicate::checkBloomFilter(const std::string& probe, const std::shared_ptr<parquet::BloomFilter>& bloom_filter){
    if (!bloom_filter) return true;  

    parquet::ByteArray arr;
    arr.ptr = reinterpret_cast<const uint8_t*>(probe.data());
    arr.len = static_cast<uint32_t>(probe.size());

    uint64_t hash        = bloom_filter->Hash(&arr);   
    bool     might_exist = bloom_filter->FindHash(hash);

    std::cout << "Bloom filter says '" << probe << "' "
              << (might_exist ? "MIGHT exist — scan row group"
                              : "definitely NOT here — skip row group")
              << '\n';

    return might_exist;
}

