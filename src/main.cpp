#include <cstdint>
#include <vector>
#include <list>
#include <iostream>
#include <array>
#include <set>
#include <memory>
#include <cassert>
#include <string>

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;


enum SIDE {RIGHT = true, LEFT = false};

struct RCCode {

    struct Elem {
        struct Chain {
            vector<uint8_t> vals;
            unsigned elem; // index of the corresponding elem in the vector
            bool right;

            Chain(unsigned elem_, bool right_) : elem(elem_), right(right_) {}
            auto push_back(uint8_t val) -> decltype(vals.push_back(val)) { return vals.push_back(val); }
            auto begin() { return vals.begin(); }
            auto begin() const { return vals.begin(); }
            auto end() { return vals.end(); }
            auto end() const { return vals.end(); }
        };

        unsigned row, col;
        Chain left;
        Chain right;
        unsigned next; // vector index of the elem whose left chain is linked to this elem right chain

        Elem(unsigned r_, unsigned c_, unsigned elem_) : row(r_), col(c_), left(elem_, false), right(elem_, true), next(elem_) {}

        Chain& operator[](bool right_) {
            if (right_) return right;
            else return left;
        }
        const Chain& operator[](bool right_) const {
            if (right_) return right;
            else return left;
        }
    };

    vector<Elem> data;

    void AddElem(unsigned r_, unsigned c_) {
        data.emplace_back(r_, c_, data.size());
    }

    size_t Size() const { return data.size(); }

    Elem& operator[](unsigned pos) { return data[pos]; }
    const Elem& operator[](unsigned pos) const { return data[pos]; }
};


struct ChainCode {

    struct Chain {

        unsigned row, col;
        vector<uint8_t> vals;

        Chain(unsigned row_, unsigned col_) : row(row_), col(col_) {}

        void AddRightChain(const RCCode::Elem::Chain& chain) {

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

        void AddLeftChain(const RCCode::Elem::Chain& chain) {

            for (auto it = chain.vals.rbegin(); it != chain.vals.rend(); it++) {
                const uint8_t& val = *it;

                uint8_t new_val = 4 - val;

                vals.push_back(new_val);
            }
        }

        const uint8_t& operator[](size_t pos) const { return vals[pos]; }
        //vector<Chain>::iterator begin() { return chains.begin(); }
        vector<uint8_t>::const_iterator begin() const { return vals.begin(); }
        //vector<Chain>::iterator end() { return chains.end(); }
        vector<uint8_t>::const_iterator end() const { return vals.end(); }

    };

    vector<Chain> chains;

    const Chain& operator[](size_t pos) const { return chains[pos]; }
    //vector<Chain>::iterator begin() { return chains.begin(); }
    vector<Chain>::const_iterator begin() const { return chains.begin(); }
    //vector<Chain>::iterator end() { return chains.end(); }
    vector<Chain>::const_iterator end() const { return chains.end(); }

    void AddChain(const RCCode& rccode, vector<bool>& used_elems, unsigned pos) {

        Chain new_chain(rccode[pos].row, rccode[pos].col);

        new_chain.AddRightChain(rccode[pos].right);

        used_elems[pos] = true;

        while (true) {

            pos = rccode[pos].next;
            new_chain.AddLeftChain(rccode[pos].left);

            if (used_elems[pos]) {
                break;
            }

            new_chain.AddRightChain(rccode[pos].right);

            used_elems[pos] = true;
        }

        chains.push_back(new_chain);
    }

    ChainCode(const RCCode& rccode) {
        vector<bool> used_elems;
        used_elems.resize(rccode.Size(), false);

        for (unsigned i = 0; i < rccode.Size(); i++) {
            if (!used_elems[i]) {
                AddChain(rccode, used_elems, i);
            }
        }
    }
};

// using Template = array<int8_t, 9>;

using Template = const int8_t[9];


struct PixelInfo {
    vector<uint8_t> links;
    bool upper_max_point = false;
    bool lower_max_point = false;
    uint8_t min_points = 0;
    bool single_pixel = false;
};


struct TemplateCheck {

    static constexpr const Template templates[33] = {

        // Chain links
        { 0, 0, -1, 1, 1, -1, -1, -1, -1 },
        { -1, -1, -1, 1, 1, -1, 0, 0, -1 },
        { 1, 0, -1, -1, 1, -1, -1, -1, -1 },
        { 1, -1, -1, 0, 1, -1, -1, -1, -1 },
        { 0, 1, -1, 0, 1, -1, -1, -1, -1 },
        { -1, 1, 0, -1, 1, 0, -1, -1, -1 },
        { -1, 0, 1, -1, 1, -1, -1, -1, -1 },
        { -1, -1, 1, -1, 1, 0, -1, -1, -1 },

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
        { 0, 0, 0, 0, 1, 1, 1, -1, -1 },
        { 0, 0, 0, 0, 1, 0, 1, -1, 1 },
        { 0, 0, 0, 0, 1, 1, 0, 1, -1 },
        { 0, 0, 0, 0, 1, 0, 1, 1, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 1, 1 },
        { 0, 0, 0, 0, 1, 1, 0, 0, 1 },
        { 0, 0, 0, 0, 1, 0, 1, 0, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 0, 1 },
        { 0, 0, 0, 0, 1, 1, 0, 0, 0 },

        { -1, -1, -1, -1, 1, 1, 1, 0, 0 },
        { -1, -1, -1, -1, 1, -1, 1, 0, 1 },

        // Single pixel
        {0, 0, 0, 0, 1, 0, 0, 0, 0}

    };


    static bool TemplateMatch(const Mat1b& mat, unsigned temp, int r, int c) {
        uint8_t i = 0;
        for (int8_t y = -1; y <= 1; y++) {
            for (int8_t x = -1; x <= 1; x++) {
                int local_row = r + y;
                int local_col = c + x;

                if (templates[temp][i] == 0) {
                    if (local_row < 0 || local_row >= mat.rows || local_col < 0 || local_col >= mat.rows || mat(local_row, local_col) == 0);
                    else return false;
                }
                else if (templates[temp][i] == 1) {
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
                info.links.push_back(i / 2);
            }

        }

        // Min points
        for (unsigned i = 0; i < 12; i++) {

            if (TemplateMatch(mat, i + 8, r, c)) {
                info.min_points++;
            }

        }

        // Max point sopra
        for (unsigned i = 0; i < 10; i++) {

            if (TemplateMatch(mat, i + 20, r, c)) {
                info.upper_max_point = true;
                break;
            }

        }

        // Max point sotto
        for (unsigned i = 0; i < 2; i++) {

            if (TemplateMatch(mat, i + 30, r, c)) {
                info.lower_max_point = true;
                break;
            }

        }

        // Single pixel
        if (TemplateMatch(mat, 32, r, c)) {
            info.single_pixel = true;
        }

        return info;
    }
};


// Connects the two chains preceding the parameter iterator and removes them from the list
// The pair<unsigned, bool> represents a chain
void ConnectChains(RCCode& rccode, list<pair<unsigned, bool>>& chains, list<pair<unsigned, bool>>::iterator& it) {

    list<pair<unsigned, bool>>::iterator first_it = it;
    first_it--;
    list<pair<unsigned, bool>>::iterator second_it = first_it;
    second_it--;

    if ((*first_it).second) {
        rccode[(*first_it).first].next = (*second_it).first;
    }
    else {
        rccode[(*second_it).first].next = (*first_it).first;
    }

    // Remove chains from list
    chains.erase(second_it, it);
}


int main(void) {

    unsigned n_tests = 100000;
    unsigned rows = 10;
    unsigned cols = 10;
    unsigned arrow_length = 50;

    Mat1b mat(rows, cols);
    // mat <<
        //1, 0, 0, 0, 1,
        //0, 1, 1, 1, 1,
        //0, 1, 1, 0, 1,
        //0, 1, 1, 1, 1,
        //1, 0, 0, 0, 0;

    RNG rng;

    string arrow = "|";
    for (unsigned i = 0; i < arrow_length; i++) {
        arrow += ' ';
    }
    arrow += '|';

    for (unsigned i = 0; i < n_tests; i++) {

        // Update arrow
        unsigned arrow_pos = i / ((n_tests) / arrow_length) + 1;
        arrow[arrow_pos] = '>';
        if (arrow[arrow_pos - 1] == '>') {
            arrow[arrow_pos - 1] = '=';
        }
        cout << '\r' << arrow;

        rng.fill(mat, RNG::UNIFORM, 0, 2);

        // RCCode building algorithm
        RCCode rccode;

        list<pair<unsigned, bool>> chains;

        for (int r = 0; r < mat.rows; r++) {
            list<pair<unsigned, bool>>::iterator it = chains.begin();

            for (int c = 0; c < mat.cols; c++) {
                PixelInfo info = TemplateCheck::Check(mat, r, c);

                vector<uint8_t>::iterator links_it = info.links.begin();

                vector<uint8_t>& links = info.links;

                bool last_found_right = false;

                bool second_link_equal_to_third_link = false;

                if (links_it != info.links.end()) {

                    // Move the list iterator back if 0 can be added to former chains
                    if ((*links_it) == 0) {
                        it--;
                        links_it++;
                        if ((links_it != links.end()) && (*links_it) == 0) {
                            it--;
                        }
                    }

                    // Run through the list of template matches
                    links_it = links.begin();

                    // Chains' links
                    uint8_t links_found = 0;
                    for (uint8_t val_prev; links.size() > 0 && links_it != links.end();) {
                        uint8_t val = (*links_it);
                        rccode[(*it).first][(*it).second].push_back(val);
                        links_found++;
                        last_found_right = (*it).second;
                        it++;
                        
                        if (links_found == 3 && val == val_prev) {
                            second_link_equal_to_third_link = true;
                        }

                        links_it++;

                        val_prev = val;
                    }

                    // Min points
                    if (info.min_points == 1) {
                        // New Min point
                        if (links.size() == 2) {
                            ConnectChains(rccode, chains, it);
                        }
                        else if (links.size() == 3) {
                            if (second_link_equal_to_third_link) {
                                list<pair<unsigned, bool>>::iterator third_chain = it;
                                third_chain--;
                                ConnectChains(rccode, chains, third_chain);
                            }
                            else {
                                ConnectChains(rccode, chains, it);
                            }
                        }
                        else {
                            list<pair<unsigned, bool>>::iterator fourth_chain = it;
                            fourth_chain--;
                            ConnectChains(rccode, chains, fourth_chain);
                        }
                    }
                    else if (info.min_points == 2) {
                        list<pair<unsigned, bool>>::iterator fourth_chain = it;
                        fourth_chain--;
                        ConnectChains(rccode, chains, fourth_chain);
                        ConnectChains(rccode, chains, it);
                    }

                }

                if (info.upper_max_point) {
                    rccode.AddElem(r, c);

                    chains.insert(it, make_pair<unsigned, bool>(static_cast<unsigned>(rccode.Size() - 1), LEFT));
                    chains.insert(it, make_pair<unsigned, bool>(static_cast<unsigned>(rccode.Size() - 1), RIGHT));

                    last_found_right = true;
                }

                if (info.lower_max_point) {
                    rccode.AddElem(r, c);

                    if (last_found_right) {
                        it--;
                    }

                    chains.insert(it, make_pair<unsigned, bool>(static_cast<unsigned>(rccode.Size() - 1), RIGHT));
                    chains.insert(it, make_pair<unsigned, bool>(static_cast<unsigned>(rccode.Size() - 1), LEFT));

                    if (last_found_right) {
                        it++;
                    }

                }

                if (info.single_pixel) {
                    rccode.AddElem(r, c);
                }

            }
        }

        // Convert to Freeman Chain Code
        ChainCode cc(rccode);

        // Convert Freeman Chain Code to Contours
        vector<vector<Point>> contours;
        for (const auto& chain : cc) {

            unsigned row = chain.row, col = chain.col;

            vector<Point> contour;

            contour.emplace_back(col, row);

            for (auto val : chain) {

                // Update col
                uint8_t tmp_col = (val + 1) % 8;
                if (tmp_col < 3) {
                    col++;
                }
                else if (3 < tmp_col && tmp_col < 7) {
                    col--;
                }

                // Update row
                uint8_t tmp_row = val;
                if (0 < tmp_row && tmp_row < 4) {
                    row--;
                }
                else if (4 < tmp_row) {
                    row++;
                }

                contour.emplace_back(col, row);
            }

            contours.push_back(contour);
        }

        Mat1b rec(mat.size(), 0);

        drawContours(rec, contours, -1, 1, FILLED);

        Mat1b diff = (mat != rec);

        bool correct = (countNonZero(diff) == 0);

        if (correct) {
            //return EXIT_SUCCESS;
        }
        else {
            return EXIT_FAILURE;
        }

    }

}