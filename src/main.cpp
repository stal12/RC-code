#include <cstdint>
#include <vector>
#include <list>
#include <iostream>
#include <array>
#include <set>
#include <memory>
#include <cassert>

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;



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
            for (int i = 1; i < contour.size(); i++) {
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

// using Template = array<int8_t, 9>;

using Template = const int8_t[9];


struct PixelInfo {
    list<uint8_t> links;
    uint8_t outer_max_points = 0;       // Il massimo è comunque 1
    uint8_t inner_max_points = 0;       // Il massimo è comunque 1
    uint8_t min_points = 0;
    bool single_pixel = false;
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

        // Min Points
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
        {0, 0, 1, 1, 1, -1, -1, -1, -1},
        {1, 0, 1, -1, 1, -1, -1, -1, -1},

        // Max Points
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

    static PixelInfo Check(const Mat1b& mat, int r, int c) {

        PixelInfo info;

        // Chains' links
        for (unsigned i = 0; i < 8; i++) {

            if (TemplateMatch(mat, i, r, c)) {
                info.links.push_back(i);
            }

        }

        // Min points
        for (unsigned i = 0; i < 12; i++) {

            if (TemplateMatch(mat, i + 8, r, c)) {
                info.min_points++;
            }

        }

        // Max points sopra
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(mat, i + 20, r, c)) {
                info.outer_max_points++;
            }

        }

        // Max points sotto
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(mat, i + 30, r, c)) {
                info.inner_max_points++;
            }

        }

        // Single pixel
        if (TemplateMatch(mat, 32, r, c)) {
            info.single_pixel = true;
        }

        return info;
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

int main(void) {

    //Mat1b mat(5, 5);
    //mat <<
    //    1, 1, 1, 0, 0,
    //    0, 0, 1, 1, 1,
    //    1, 1, 0, 0, 1,
    //    0, 0, 1, 1, 1,
    //    1, 0, 0, 0, 1;

    Mat1b mat(5, 5);

    for (unsigned int i = 0; i < (1 << 25); i++) {

        // Fill mat
        for (unsigned int pos = 0; pos < 25; pos++) {
            mat.data[pos] = (i >> pos) & 1u;
        }

        //mat <<
        //    1, 0, 1,
        //    1, 0, 1,
        //    1, 1, 1;

        vector<unique_ptr<RCCode>> RCCodes;

        list<RCCode::chain*> chains;

        // Build Raster Scan Chain Code
        for (int r = 0; r < mat.rows; r++) {
            list<RCCode::chain*>::iterator it = chains.begin();

            // how many former chains can receive horizontal (0) links
            uint8_t back = 0;

            for (int c = 0; c < mat.cols; c++) {
                PixelInfo info = TemplateCheck::Check(mat, r, c);

                list<uint8_t>& l = info.links;

                list<uint8_t>::iterator match_it = l.begin();

                bool last_found_right = false;

                bool second_link_equal_to_third_link = false;

                if (match_it != info.links.end()) {

                    // Move the list iterator back if 0 can be added to former chains
                    if ((*match_it) / 2 == 0) {
                        it--;
                        match_it++;
                        if ((match_it != l.end()) && (*match_it) / 2 == 0) {
                            it--;
                        }
                    }
                    back = 0;

                    // Run through the list of template matches
                    match_it = l.begin();

                    // Chains' links
                    uint8_t links_found = 0;
                    while (l.size() > 0 && match_it != l.end()) {
                        uint8_t val = (*match_it);
                        if (val > (4 * 2)) {
                            break;
                        }
                        (*it)->push_back(val / 2);
                        back++;
                        links_found++;
                        last_found_right = (*it)->right;
                        it++;

                        // Detect and resolve Min point
                        list<uint8_t>::iterator match_it_cur = l.end();

                        if (match_it != l.begin()) {
                            match_it_cur = match_it;
                        }

                        match_it++;

                        if (links_found > 2 && match_it_cur != l.end()) {
                            list<uint8_t>::iterator match_it_prev = match_it_cur;
                            match_it_prev--;
                            if ((val / 2) != ((*match_it_prev) / 2)) {

                                if (info.min_points > 0) {

                                    ConnectChains(chains, it);

                                    // Remove values from match list
                                    l.erase(match_it_prev, match_it);

                                    // Remove Min point from info
                                    info.min_points--;

                                    back -= 2;
                                }
                            }
                            else {
                                second_link_equal_to_third_link = true;
                            }
                        }

                    }

                    assert(info.min_points < 2);

                    if (info.min_points == 1) {
                        // New Min point
                        if (l.size() == 2) {
                            ConnectChains(chains, it);
                        }
                        if (l.size() == 3) {
                            if (second_link_equal_to_third_link) {
                                list<RCCode::chain*>::iterator third_chain = it;
                                third_chain--;
                                ConnectChains(chains, third_chain);
                                back -= 2;
                                info.min_points--;
                            }
                            else {
                                ConnectChains(chains, it);
                            }
                        }
                        back -= 2;
                        info.min_points--;
                    }

                }

                while (info.outer_max_points > 0) {
                    RCCodes.push_back(make_unique<RCCode>(r, c));

                    if (back > 0 && last_found_right) {
                        it--;
                    }

                    chains.insert(it, &(RCCodes.back().get()->left));
                    chains.insert(it, &(RCCodes.back().get()->right));

                    if (back > 0 && last_found_right) {
                        it++;
                    }

                    last_found_right = true;
                    back += 2;
                    info.outer_max_points--;
                }

                while (info.inner_max_points > 0) {
                    RCCodes.push_back(make_unique<RCCode>(r, c));

                    if (back > 0 && last_found_right) {
                        it--;
                    }

                    chains.insert(it, &(RCCodes.back().get()->right));
                    chains.insert(it, &(RCCodes.back().get()->left));

                    if (back > 0 && last_found_right) {
                        it++;
                    }

                    back += 2;
                    info.inner_max_points--;
                }

                if (info.single_pixel) {
                    RCCodes.push_back(make_unique<RCCode>(r, c));
                }

            }
        }

        // Convert to Freeman Chain Code
        ChainCode cc(RCCodes);

        vector<vector<Point>> cv_contours;
        vector<Vec4i> hierarchy;
        findContours(mat, cv_contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_NONE);
        ChainCode cc_2(cv_contours);

        if (!(cc == cc_2)) {
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}