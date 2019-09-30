#include <cstdint>
#include <vector>
#include <list>
#include <iostream>
#include <array>
#include <set>
#include <memory>
#include <cassert>
#include <fstream>

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

// The state (PixelInfo) can be repsented as a bitmapped unsigned short.
// Meaning of bits is the following:
// 0  - D0a
// 1  - D0b
// 2  - D1a
// 3  - D1b
// 4  - D2a
// 5  - D2b
// 6  - D3a
// 7  - D3b
// 8  - Min outer
// 9  - Min inner
// 10 - Max outer
// 11 - Max inner
// 12 - Single pixel

#define D0A             1u
#define D0B             2u
#define D1A             4u
#define D1B             8u
#define D2A             16u
#define D2B             32u
#define D3A             64u
#define D3B             128u
#define MIN_OUTER       256u
#define MIN_INNER       512u
#define MAX_OUTER       1024u
#define MAX_INNER       2048u
#define SINGLE_PIXEL    4096u

struct RCCode {

    struct chain {
        vector<uint8_t> vals;
        RCCode* const maxpoint;
        bool right;

        chain(RCCode* RCCode_, bool right_) : maxpoint(RCCode_), right(right_) {}
        auto push_back(uint8_t val) -> decltype(vals.push_back(val)) { return vals.push_back(val); }
        auto begin() { return vals.begin(); }
        auto begin() const { return vals.begin(); }
        auto end() { return vals.end(); }
        auto end() const { return vals.end(); }
    };

    unsigned row, col;
    chain left;
    chain right;
    RCCode* next = nullptr;
    bool used = false;

    RCCode(unsigned r_, unsigned c_) : row(r_), col(c_), left(this, false), right(this, true), next(this) {}
};


struct ChainCode {

    struct Chain {

        unsigned row, col;
        vector<uint8_t> vals;

        Chain(unsigned row_, unsigned col_) : row(row_), col(col_) {}

        bool operator==(const Chain& rhs) const {
            return row == rhs.row &&
                col == rhs.col &&
                vals == rhs.vals;
        }

        bool operator<(const Chain& rhs) const {
            if (row != rhs.row) {
                return row < rhs.row;
            }
            else if (col != rhs.col) {
                return col < rhs.col;
            }
            else if (vals.size() != rhs.vals.size()) {
                return vals.size() < rhs.vals.size();
            }
            else {
                return vals[0] < rhs.vals[0];
            }
        }

        void AddRightChain(const RCCode::chain& chain) {

            for (auto val : chain.vals) {
                uint8_t new_val;
                if (val == 0) {
                    new_val = 0;
                }
                else {
                    new_val = 8 - val;
                }
                vals.push_back(new_val);
            }

        }

        void AddLeftChain(const RCCode::chain& chain) {

            for (auto it = chain.vals.rbegin(); it != chain.vals.rend(); it++) {
                const uint8_t& val = *it;

                uint8_t new_val = 4 - val;

                vals.push_back(new_val);
            }
        }

    };

    set<Chain> chains;

    // Attenzione! Modifica il RCCode.
    void AddChain(RCCode* rccode) {

        Chain new_chain(rccode->row, rccode->col);

        new_chain.AddRightChain(rccode->right);

        rccode->used = true;

        while (true) {

            rccode = rccode->next;
            new_chain.AddLeftChain(rccode->left);

            if (rccode->used) {
                break;
            }

            new_chain.AddRightChain(rccode->right);
            rccode->used = true;
        }

        chains.insert(new_chain);
    }

    // Attenzione! Modifica i RCCode.
    ChainCode(const vector<unique_ptr<RCCode>>& rccodes) {
        for (const unique_ptr<RCCode>& RCCode_ptr : rccodes) {
            if (!RCCode_ptr->used) {
                AddChain(RCCode_ptr.get());
            }
        }
    }

    ChainCode(const vector<vector<Point>>& contours) {
        for (const vector<Point>& contour : contours) {

            // Find top-left value
            int top = 0;
            Point max = contour.front();
            for (unsigned int i = 1; i < contour.size(); i++) {
                if (contour[i].y < max.y || (contour[i].y == max.y && contour[i].x < max.x)) {
                    max = contour[i];
                    top = i;
                }
            }

            Chain chain(max.y, max.x);
            Point prev = max;

            if (contour.size() > 1) {

                for (int i = top - 1 + contour.size(); i >= top; i--) { // OpenCV order is reversed

                    Point cur = contour[i % contour.size()];
                    Point diff = cur - prev;

                    int link;
                    if (diff.x == -1) {
                        link = diff.y + 4;
                    }
                    else if (diff.x == 0) {
                        link = diff.y * 2 + 4;
                    }
                    else if (diff.x == 1) {
                        link = (8 - diff.y) % 8;
                    }

                    chain.vals.push_back(link);

                    prev = cur;

                }

            }

            chains.insert(chain);
        }
    }

    bool operator==(const ChainCode& rhs) const {
        return chains == rhs.chains;
    }

};

using Template = const int8_t[9];

struct PixelInfo {
    array<bool, 8> links = { false, false, false, false, false, false, false, false };
    bool outer_max_point = false;
    bool inner_max_point = false;
    uint8_t min_points = 0;
    bool single_pixel = false;
    uint8_t links_num = 0;
};


struct TemplateCheck {

    static constexpr const Template templates[33] = {

        // Chain links
        { 0,  0, -1,
          1,  1, -1,
         -1, -1, -1,
        },
        {-1, -1, -1,
          1,  1, -1,
          0,  0, -1,
        },
        { 1,  0, -1,
         -1,  1, -1,
         -1, -1, -1
        },
        { 1, -1, -1,
          0,  1, -1,
         -1, -1, -1
        },
        { 0,  1, -1,
          0,  1, -1,
         -1, -1, -1
        },
        {-1,  1,  0,
         -1,  1,  0,
         -1, -1, -1
        },
        {-1,  0,  1,
         -1,  1, -1,
         -1, -1, -1
        },
        {-1, -1,  1,
         -1,  1,  0,
         -1, -1, -1
        },

        // Min Points (outer)
        {-1, -1, 1, 1, 1, 0, 0, 0, 0},
        {1, -1, 1, 0, 1, 0, 0, 0, 0},
        {-1, 1, 0, 1, 1, 0, 0, 0, 0},
        {0, 1, 1, 0, 1, 0, 0, 0, 0},
        {1, 1, 0, 0, 1, 0, 0, 0, 0},
        {1, 0, 0, 1, 1, 0, 0, 0, 0},
        {0, 0, 1, 0, 1, 0, 0, 0, 0},
        {0, 1, 0, 0, 1, 0, 0, 0, 0},
        {1, 0, 0, 0, 1, 0, 0, 0, 0},
        {0, 0, 0, 1, 1, 0, 0, 0, 0},

        // Min Points (inner)
        {0, 0, 1, 1, 1, -1, -1, -1, -1},
        {1, 0, 1, -1, 1, -1, -1, -1, -1},

        // Max Points (outer)
        {0,  0,  0,
         0,  1,  1,
         1, -1, -1
        },
        {0,  0,  0,
         0,  1,  0,
         1, -1,  1
        },
        {0,  0,  0,
         0,  1,  1,
         0,  1, -1
        },
        {0,  0,  0,
         0,  1,  0,
         1,  1,  0
        },
        {0,  0,  0,
         0,  1,  0,
         0,  1,  1
        },
        {0,  0,  0,
         0,  1,  1,
         0,  0,  1
        },
        {0,  0,  0,
         0,  1,  0,
         1,  0,  0
        },
        {0,  0,  0,
         0,  1,  0,
         0,  1,  0
        },
        {0,  0,  0,
         0,  1,  0,
         0,  0,  1
        },
        {0,  0,  0,
         0,  1,  1,
         0,  0,  0
        },

        // Max Points (inner)
        { -1, -1, -1, -1, 1, 1, 1, 0, 0 },
        { -1, -1, -1, -1, 1, -1, 1, 0, 1 },

        // Single pixel
        {0, 0, 0,
         0, 1, 0,
         0, 0, 0}

    };

    static bool TemplateMatch(const Mat1b& mat, unsigned templ, int r, int c) {
        uint8_t i = 0;
        for (int8_t y = -1; y <= 1; y++) {
            for (int8_t x = -1; x <= 1; x++) {
                int local_row = r + y;
                int local_col = c + x;

                if (templates[templ][i] == 0) {
                    if (local_row < 0 || local_row >= mat.rows || local_col < 0 || local_col >= mat.rows || mat(local_row, local_col) == 0);
                    else return false;
                }
                else if (templates[templ][i] == 1) {
                    if (local_row < 0 || local_row >= mat.rows || local_col < 0 || local_col >= mat.rows || mat(local_row, local_col) != 1)
                        return false;
                }

                i++;
            }
        }
        return true;
    }

    static bool TemplateMatch(unsigned short condition, unsigned templ) {
        for (uint8_t cond_i = 7, templ_i = 0; templ_i < 9; templ_i++) {

            if (templates[templ][templ_i] != -1) {

                if (templ_i == 4) {
                    // x
                    if (((condition >> 8) & 1u) != templates[templ][templ_i]) {
                        return false;
                    }
                }

                else {
                    // abcdefgh
                    if (((condition >> cond_i) & 1u) != templates[templ][templ_i]) {
                        return false;
                    }
                }
            }

            if (templ_i != 4) {
                cond_i--;
            }
        }

        return true;
    }

    static PixelInfo Check(const Mat1b& mat, int r, int c) {

        PixelInfo info;

        // Chains' links
        for (unsigned i = 0; i < 8; i++) {

            if (TemplateMatch(mat, i, r, c)) {
                info.links[i] = true;
                info.links_num++;
            }

        }

        // Min points (outer)
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(mat, i + 8, r, c)) {
                info.min_points++;
            }

        }

        // Min points (inner)
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(mat, i + 18, r, c)) {
                info.min_points++;
            }

        }

        // Max points (outer)
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(mat, i + 20, r, c)) {
                info.outer_max_point = true;
            }

        }

        // Max points (inner)
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(mat, i + 30, r, c)) {
                info.inner_max_point = true;
            }

        }

        // Single pixel
        if (TemplateMatch(mat, 32, r, c)) {
            info.single_pixel = true;
        }

        return info;
    }

    static unsigned short CheckState(const Mat1b& mat, int r, int c) {

        unsigned short state = 0;

        // Chains' links
        for (unsigned i = 0; i < 8; i++) {

            if (TemplateMatch(mat, i, r, c)) {
                state |= (1 << i);
            }

        }

        // Min points (outer)
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(mat, i + 8, r, c)) {
                state |= MIN_OUTER;
                break;
            }

        }

        // Min points (inner)
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(mat, i + 18, r, c)) {
                state |= MIN_INNER;
                break;
            }

        }

        // Max points (outer)
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(mat, i + 20, r, c)) {
                state |= MAX_OUTER;
                break;
            }

        }

        // Max points (inner)
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(mat, i + 30, r, c)) {
                state |= MAX_INNER;
                break;
            }

        }

        // Single pixel
        if (TemplateMatch(mat, 32, r, c)) {
            state |= SINGLE_PIXEL;
        }

        return state;
    }

    static unsigned short CheckState(unsigned short condition) {

        unsigned short state = 0;

        // Chains' links
        for (unsigned i = 0; i < 8; i++) {

            if (TemplateMatch(condition, i)) {
                state |= (1 << i);
            }

        }

        // Min points (outer)
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(condition, i + 8)) {
                state |= MIN_OUTER;
                break;
            }

        }

        // Min points (inner)
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(condition, i + 18)) {
                state |= MIN_INNER;
                break;
            }

        }

        // Max points (outer)
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(condition, i + 20)) {
                state |= MAX_OUTER;
                break;
            }

        }

        // Max points (inner)
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(condition, i + 30)) {
                state |= MAX_INNER;
                break;
            }

        }

        // Single pixel
        if (TemplateMatch(condition, 32)) {
            state |= SINGLE_PIXEL;
        }

        return state;
    }

};

// Connect the two chains preceding the parameter iterator and removes them from the list
void ConnectChains(list<RCCode::chain*>& chains, list<RCCode::chain*>::iterator& it) {

    list<RCCode::chain*>::iterator first_it = it;
    first_it--;
    list<RCCode::chain*>::iterator second_it = first_it;
    second_it--;

    if ((*first_it)->right) {
        (*first_it)->maxpoint->next = (*second_it)->maxpoint;
    }
    else {
        (*second_it)->maxpoint->next = (*first_it)->maxpoint;
    }

    // Remove chains from list
    chains.erase(second_it, it);
}

void ProcessPixel(int r, int c, unsigned short state, vector<unique_ptr<RCCode>>& RCCodes, list<RCCode::chain*>& chains, list<RCCode::chain*>::iterator& it) {

    bool last_found_right = false;

    if ((state & D0A) || (state & D0B)) {
        it--;
    }
    if ((state & D0A) && (state & D0B)) {
        it--;
    }

    // Chains' links
    uint8_t links_found = 0;

    if (state & D0A) {
        (*it)->push_back(0);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D0B) {
        (*it)->push_back(0);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D1A) {
        (*it)->push_back(1);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D1B) {
        (*it)->push_back(1);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D2A) {
        (*it)->push_back(2);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D2B) {
        (*it)->push_back(2);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D3A) {
        (*it)->push_back(3);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    if (state & D3B) {
        (*it)->push_back(3);
        links_found++;
        last_found_right = (*it)->right;
        it++;
    }

    // Min points
    if ((state & MIN_INNER) && (state & MIN_OUTER)) {
        list<RCCode::chain*>::iterator fourth_chain = it;
        fourth_chain--;
        ConnectChains(chains, fourth_chain);
        ConnectChains(chains, it);
    }
    else if ((state & MIN_INNER) || (state & MIN_OUTER)) {
        if (links_found == 2) {
            ConnectChains(chains, it);
        }
        else if (links_found == 3) {
            if ((state & D3A) && (state & D3B)) {
                // Connect first two chains met
                list<RCCode::chain*>::iterator third_chain = it;
                third_chain--;
                ConnectChains(chains, third_chain);
            }
            else {
                // Connect last two chains met
                ConnectChains(chains, it);
            }
        }
        else {
            list<RCCode::chain*>::iterator fourth_chain = it;
            fourth_chain--;
            ConnectChains(chains, fourth_chain);
        }
    }

    if (state & MAX_OUTER) {
        RCCodes.push_back(make_unique<RCCode>(r, c));

        chains.insert(it, &(RCCodes.back().get()->left));
        chains.insert(it, &(RCCodes.back().get()->right));

        last_found_right = true;
    }

    if (state & MAX_INNER) {
        RCCodes.push_back(make_unique<RCCode>(r, c));

        if (last_found_right) {
            it--;
        }

        chains.insert(it, &(RCCodes.back().get()->right));
        chains.insert(it, &(RCCodes.back().get()->left));

        if (last_found_right) {
            it++;
        }

    }

    if (state & SINGLE_PIXEL) {
        RCCodes.push_back(make_unique<RCCode>(r, c));
    }

}


/*

    a | b | c
   ---+---+---
    d | x | e
   ---+---+---
    f | g | h

    The condition outcome of the mask is represented as a bitmapped unsigned short (16 bit), where the first 9 bits are used.

    bit       8 7 6 5 4 3 2 1 0
    pixel     x a b c d e f g h

    Every condition outcome is linked to a state by means of a look-up table, which is a 512-sized array of unsigned short.

*/

#define PIXEL_H     1u
#define PIXEL_G     2u
#define PIXEL_F     4u
#define PIXEL_E     8u
#define PIXEL_D     16u
#define PIXEL_C     32u
#define PIXEL_B     64u
#define PIXEL_A     128u
#define PIXEL_X     256u


bool FillLookupTable(string filename = "fill_lookup_table.inc", string table_name = "table") {
    
    ofstream os(filename);
    if (!os) {
        return false;
    }

    for (unsigned short i = 0; i < 512; i++) {

        unsigned short state = TemplateCheck::CheckState(i);

        os << table_name << "[" << i << "] = " << state << ";\n";

    }

    return true;
}

vector<unique_ptr<RCCode>> BuildRCCodes_LookUptable(const Mat1b& mat, const array<unsigned short, 512>& table) {

    vector<unique_ptr<RCCode>> RCCodes;
    list<RCCode::chain*> chains;

    const unsigned char* previous_row_ptr = nullptr;
    const unsigned char* row_ptr = mat.ptr(0);
    const unsigned char* next_row_ptr = row_ptr + mat.step[0];

    // Build Raster Scan Chain Code
    for (int r = 0; r < mat.rows; r++) {
        list<RCCode::chain*>::iterator it = chains.begin();

        for (int c = 0; c < mat.cols; c++) {

            unsigned short condition = 0;

            if (r > 0 && c > 0 && previous_row_ptr[c - 1])                      condition |= PIXEL_A;
            if (r > 0 && previous_row_ptr[c])                                   condition |= PIXEL_B;
            if (r > 0 && c + 1 < mat.cols && previous_row_ptr[c + 1])           condition |= PIXEL_C;
            if (c > 0 && row_ptr[c - 1])                                        condition |= PIXEL_D;
            if (row_ptr[c])                                                     condition |= PIXEL_X;
            if (c + 1 < mat.cols && row_ptr[c + 1])                             condition |= PIXEL_E;
            if (r + 1 < mat.rows && c > 0 && next_row_ptr[c - 1])               condition |= PIXEL_F;
            if (r + 1 < mat.rows && next_row_ptr[c])                            condition |= PIXEL_G;
            if (r + 1 < mat.rows && c + 1 < mat.cols && next_row_ptr[c + 1])    condition |= PIXEL_H;

            unsigned short state = table[condition];

            ProcessPixel(r, c, state, RCCodes, chains, it);
        }

        previous_row_ptr = row_ptr;
        row_ptr = next_row_ptr;
        next_row_ptr += mat.step[0];
    }

    return RCCodes;
}

vector<unique_ptr<RCCode>> BuildRCCodes_LookUpTable_Prediction(const Mat1b& mat, const array<unsigned short, 512> & table) {

    vector<unique_ptr<RCCode>> RCCodes;
    list<RCCode::chain*> chains;

    const unsigned char* previous_row_ptr = nullptr;
    const unsigned char* row_ptr = mat.ptr(0);
    const unsigned char* next_row_ptr = row_ptr + mat.step[0];

    // Build Raster Scan Chain Code
    for (int r = 0; r < mat.rows; r++) {
        list<RCCode::chain*>::iterator it = chains.begin();
        
        unsigned short condition = 0;
        unsigned short old_condition = 0;

        // First column
        if (r > 0 && previous_row_ptr[0])                           condition |= PIXEL_B;
        if (r > 0 && 1 < mat.cols && previous_row_ptr[1])           condition |= PIXEL_C;
        if (row_ptr[0])                                             condition |= PIXEL_X;
        if (1 < mat.cols && row_ptr[1])                             condition |= PIXEL_E;
        if (r + 1 < mat.rows && next_row_ptr[0])                    condition |= PIXEL_G;
        if (r + 1 < mat.rows && 1 < mat.cols && next_row_ptr[1])    condition |= PIXEL_H;

        unsigned short state = table[condition];
        ProcessPixel(r, 0, state, RCCodes, chains, it);
        old_condition = condition;

        // Middle columns
        for (int c = 1; c < mat.cols - 1; c++) {

            condition = 0;
            condition |= ((old_condition & PIXEL_B) << 1);  // A <- B
            condition |= ((old_condition & PIXEL_X) >> 4);  // D <- X
            condition |= ((old_condition & PIXEL_G) << 1);  // F <- G
            condition |= ((old_condition & PIXEL_C) << 1);  // B <- C
            condition |= ((old_condition & PIXEL_E) << 5);  // X <- E
            condition |= ((old_condition & PIXEL_H) << 1);  // G <- H

            if (r > 0 && previous_row_ptr[c + 1])           condition |= PIXEL_C;
            if (row_ptr[c + 1])                             condition |= PIXEL_E;
            if (r + 1 < mat.rows && next_row_ptr[c + 1])    condition |= PIXEL_H;

            state = table[condition];
            ProcessPixel(r, c, state, RCCodes, chains, it);
            old_condition = condition;
        }

        // Last column
        condition = 0;
        condition |= ((old_condition & PIXEL_B) << 1);  // A <- B
        condition |= ((old_condition & PIXEL_X) >> 4);  // D <- X
        condition |= ((old_condition & PIXEL_G) << 1);  // F <- G
        condition |= ((old_condition & PIXEL_C) << 1);  // B <- C
        condition |= ((old_condition & PIXEL_E) << 5);  // X <- E
        condition |= ((old_condition & PIXEL_H) << 1);  // G <- H

        state = table[condition];
        ProcessPixel(r, mat.cols - 1, state, RCCodes, chains, it);

        previous_row_ptr = row_ptr;
        row_ptr = next_row_ptr;
        next_row_ptr += mat.step[0];
    }

    return RCCodes;
}



int main(void) {

    const int kSize = 3;

    Mat1b mat(kSize, kSize);

    array<unsigned short, 512> table;

#include "fill_lookup_table.inc"

    for (unsigned int i = 0; i < (1 << (kSize * kSize)); i++) {

        // Fill mat
        for (unsigned int pos = 0; pos < (kSize * kSize); pos++) {
            mat.data[pos] = (i >> pos) & 1u;
        }

        //mat <<
        //    1, 0, 1,
        //    1, 0, 1,
        //    1, 1, 1;

        vector<unique_ptr<RCCode>> RCCodes = BuildRCCodes_LookUpTable_Prediction(mat, table);

        // Convert to Freeman Chain Code
        ChainCode cc(RCCodes);

        vector<vector<Point>> cv_contours;
        findContours(mat, cv_contours, RETR_LIST, CHAIN_APPROX_NONE);
        ChainCode cc_2(cv_contours);

        if (!(cc == cc_2)) {
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}


#undef PIXEL_H
#undef PIXEL_G
#undef PIXEL_F
#undef PIXEL_E
#undef PIXEL_D
#undef PIXEL_C
#undef PIXEL_B
#undef PIXEL_A
#undef PIXEL_X

#undef D0A         
#undef D0B         
#undef D1A         
#undef D1B         
#undef D2A         
#undef D2B         
#undef D3A         
#undef D3B         
#undef MIN_OUTER   
#undef MIN_INNER   
#undef MAX_OUTER   
#undef MAX_INNER   
#undef SINGLE_PIXEL