#ifndef FSALIGN_TYPES_H
#define FSALIGN_TYPES_H

#include <opencv2/core.hpp>

struct Result
{
    // Affine transform mapping source → reference/global
    // 2x3 CV_32F
    cv::Mat transform;

    // Contrast polynomial: [C, X, X2, Y, Y2]^T
    // 5x1 CV_32F
    cv::Mat contrast;

    // White balance: [bb, bc, gb, gc, rb, rc]^T
    // 6x1 CV_32F
    cv::Mat whitebalance;

    // Valid area of the *source image* after this transform
    cv::Rect validArea;

    Result()
    {
        transform.create(2, 3, CV_32F);
        transform = 0.0f;
        transform.at<float>(0, 0) = 1.0f;
        transform.at<float>(1, 1) = 1.0f;

        contrast.create(5, 1, CV_32F);
        contrast = 0.0f;
        contrast.at<float>(0, 0) = 1.0f;

        whitebalance.create(6, 1, CV_32F);
        whitebalance = 0.0f;
        whitebalance.at<float>(1, 0) = 1.0f;
        whitebalance.at<float>(3, 0) = 1.0f;
        whitebalance.at<float>(5, 0) = 1.0f;

        validArea = cv::Rect();
    }
};

#endif // FSALIGN_TYPES_H
