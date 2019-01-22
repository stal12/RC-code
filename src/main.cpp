#include <cstdint>

#include <iostream>

#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;


/*
Draws recursively a Hilbert Space-filling curve on a Mat1b of correct size
x, y: cohordinates of first pixel of current square
l: side of current square
d: distance between lines of the smallest C
o: orientation of the current square:
        0            ____          ____         ____
      |    |       1     |        |    |       |      3
      |____|         ____|        |    |       |____
                                      2
*/

void DrawHilbertCurveRec(Mat1b& mat, unsigned x, unsigned y, unsigned l, unsigned d, unsigned char o) {

    if (o == 0) {
        for (unsigned c = x + (l - d - 2) / 2; c < (x + (l - d - 2) / 2 + d + 2); c++) {
            mat(y + (l + d) / 2, c) = 1;
        }
    }
    if (o == 1) {
        for (unsigned r = y + (l - d - 2) / 2; r < (y + (l - d - 2) / 2 + d + 2); r++) {
            mat(r, x + (l + d) / 2) = 1;
        }
    }
    if (o == 2) {
        for (unsigned c = x + (l - d - 2) / 2; c < (x + (l - d - 2) / 2 + d + 2); c++) {
            mat(y + (l - d) / 2 - 1, c) = 1;
        }
    }
    if (o == 3) {
        for (unsigned r = y + (l - d - 2) / 2; r < (y + (l - d - 2) / 2 + d + 2); r++) {
            mat(r, x + (l - d) / 2 - 1) = 1;
        }
    }

    if (o == 0 || o == 2) {
        for (unsigned r = y + (l - d - 2) / 2; r < (y + (l - d - 2) / 2 + d + 2); r++) {
            mat(r, x) = 1;
            mat(r, x + l - 1) = 1;
        }
    }
    if (o == 1 || o == 3) {
        for (unsigned c = x + (l - d - 2) / 2; c < (x + (l - d - 2) / 2 + d + 2); c++) {
            mat(y, c) = 1;
            mat(y + l - 1, c) = 1;
        }
    }


    if (d + 2 < l) {

        unsigned char o0 = o, o1 = o, o2 = o, o3 = o;

        if (o == 0) {
            o0 = 1;
            o1 = 3;
        }
        if (o == 1) {
            o0 = 0;
            o2 = 2;
        }
        if (o == 2) {
            o2 = 1;
            o3 = 3;
        }
        if (o == 3) {
            o1 = 0;
            o3 = 2;
        }

        DrawHilbertCurveRec(mat, x, y, (l - d) / 2, d, o0);
        DrawHilbertCurveRec(mat, x + (l - d) / 2 + d, y, (l - d) / 2, d, o1);
        DrawHilbertCurveRec(mat, x, y + (l - d) / 2 + d, (l - d) / 2, d, o2);
        DrawHilbertCurveRec(mat, x + (l - d) / 2 + d, y + (l - d) / 2 + d, (l - d) / 2, d, o3);
    }
}

/*
Draws a Hilbert Space-filling curve on a Mat1b and returns it
d: distance between lines of the smallest C
it: number of iterations
o: orientation of the biggest square:
        0            ____          ____         ____
      |    |       1     |        |    |       |      3
      |____|         ____|        |    |       |____
                                      2
*/
Mat1b DrawHilbertCurve(unsigned d, unsigned it, unsigned char o = 0) {

    unsigned l = 1;
    for (unsigned i = 0; i < it; i++) {
        l = l * 2 + d;
    }

    unsigned dim = l + ((d + 1) / 2) * 2;
    unsigned x = (d + 1) / 2;
    unsigned y = (d + 1) / 2;

    Mat1b mat(dim, dim, (uchar)0);

    DrawHilbertCurveRec(mat, x, y, l, d, 0);

    return mat;
}


int main(int argc, char **argv) {

    

    unsigned d = 4;
    unsigned it = 4;

    Mat1b curve = DrawHilbertCurve(d, it);

    imwrite("curve.png", curve, { IMWRITE_PNG_BILEVEL, 1 });

    return EXIT_SUCCESS;
}
