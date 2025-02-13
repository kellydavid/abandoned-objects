//
//  utilities.hpp
//  abandoned-objects
//
//  Created by David Kelly on 08/12/2015.
//  Copyright © 2015 David Kelly. All rights reserved.
//

#ifndef utilities_hpp
#define utilities_hpp

#include <stdio.h>
#include <iostream>
#include "opencv2/core.hpp"

/******* Image operation helper functions *******/

// returns binary image
void otsu_threshold(cv::Mat *image);

// can be used on a binary image
void binary_closing_operation(cv::Mat *image);

void binary_opening_operation(cv::Mat *image);

// back projects the colour sample
cv::Mat back_project(cv::Mat image, cv::Mat colour_sample, int number_bins);

/******* Image Display Functions ******************/

void DisplayImage(cv::Mat image, std::string message, int x, int y);

void draw_points(cv::Mat *image, std::vector<cv::Point2f> points);

cv::Mat rescaleImage(cv::Mat image, double scale);

cv::Mat JoinImagesHorizontally( cv::Mat& image1, std::string name1, cv::Mat& image2, std::string name2, int spacing, cv::Scalar passed_colour );

void writeText( cv::Mat image, std::string text, int row, int column, cv::Scalar passed_colour, double scale=0.5, int thickness=1.0 );

bool load_image(std::string filename, cv::Mat *image);

// increases the rect by given size evenly on its length and width
cv::Rect increaseRectSize(cv::Rect rect, int size);

// decreases the rect by given size evenly on its length and width
cv::Rect decreaseRectSize(cv::Rect rect, int size);

int getOverlapArea(cv::Rect rect1, cv::Rect rect2);

#endif /* utilities_hpp */
