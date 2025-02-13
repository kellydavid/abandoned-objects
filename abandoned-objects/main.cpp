//
//  main.cpp
//  abandoned-objects
//
//  Created by David Kelly on 08/12/2015.
//  Copyright © 2015 David Kelly. All rights reserved.
//

#include <iostream>
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "utilities.hpp"
#include "video.hpp"
#include "world_object.hpp"
#include "system_perf.hpp"

#define MB_LEARN_RATE_ONE 1.001
#define MB_LEARN_RATE_TWO 1.01
#define MB_NUMBER_BINS 4
#define MEDIAN_DIFFERENCE_THRESHOLD 50

using namespace std;
using namespace cv;

SystemPerf ground_truth[NUMBER_VIDEOS] = {
    *new SystemPerf(VIDEO_ONE, VIDEO_ONE_FRAME_RATE, VIDEO_ONE_TOTAL_FRAMES),
    *new SystemPerf(VIDEO_TWO, VIDEO_TWO_FRAME_RATE, VIDEO_TWO_TOTAL_FRAMES)
};

WorldObjectManager observed_truth[NUMBER_VIDEOS];

// finds the objects in the video
int find_objects(VideoCapture video, WorldObjectManager &woManager, float learn_rate_one, float learn_rate_two, int bins);

// computes the performance metrics
void compute_metrics(WorldObjectManager &woManager, SystemPerf &groundTruth);

// intitialises the ground truth array
void setup_ground_truth_events();

int main(int argc, const char * argv[]) {
    setup_ground_truth_events();
    for(int i = 0; i < NUMBER_VIDEOS; i++){
        cout << "Video #" << i + 1 << endl;
        VideoCapture video = *new VideoCapture(ground_truth[i].getVideoFile());
        observed_truth[i] = *new WorldObjectManager();
        int res = find_objects(video, observed_truth[i], MB_LEARN_RATE_ONE, MB_LEARN_RATE_TWO, MB_NUMBER_BINS);
        if(res == -1){
            return -1;
        }
        compute_metrics(observed_truth[i], ground_truth[i]);
        cout << endl << endl;
    }
    return 0;
}

int find_objects(VideoCapture video, WorldObjectManager &woManager, float learn_rate_one, float learn_rate_two, int bins){
    if(!video.isOpened()){
        cout << "Error: Video not opened." << endl;
        return -1;
    }
    Mat current_frame, medianBgImageOne, medianBgImageTwo, medianDifference;
    video.retrieve(current_frame);
    woManager.setOriginalBackgroundImage(current_frame.clone());
    MedianBackground medianBgOne = MedianBackground(current_frame, learn_rate_one, bins);
    MedianBackground medianBgTwo = MedianBackground(current_frame, learn_rate_two, bins);
    namedWindow("video");
    namedWindow("difference");
    for(int i = 0; i < (int)video.get(CV_CAP_PROP_FRAME_COUNT); i++){
        
        // updates the current frame and retrieves it
        video.set(CV_CAP_PROP_POS_FRAMES, i);
        video.retrieve(current_frame);
        
        // update the median background images
        medianBgOne.UpdateBackground(current_frame);
        medianBgTwo.UpdateBackground(current_frame);
        medianBgImageOne = medianBgOne.GetBackgroundImage();
        medianBgImageTwo = medianBgTwo.GetBackgroundImage();
        
        // get the binary difference image
        absdiff(medianBgImageOne, medianBgImageTwo, medianDifference);
        cvtColor(medianDifference, medianDifference, CV_BGR2GRAY);
        threshold(medianDifference, medianDifference, MEDIAN_DIFFERENCE_THRESHOLD, 255, THRESH_BINARY);
        
        // clean the binary difference image
        binary_closing_operation(&medianDifference);
        binary_opening_operation(&medianDifference);
        
        // get object region contours
        Mat medianDifferenceTemp = medianDifference.clone();
        vector<vector<Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(medianDifferenceTemp, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_NONE);
        medianDifferenceTemp.release();
        
        // update world object manager if there are detected object regions
        woManager.update(contours, current_frame.clone(), i);
        woManager.drawCurrentObjectRegions(current_frame);
        
        // Update Window
        writeText(current_frame, "Frame: " + to_string(i), 15, 10, Scalar(0, 255, 0));
        writeText(current_frame, "Current Objects: " + to_string((int)woManager.getCurrentObjects().size()), 35, 10, Scalar(0, 255, 0));
        writeText(current_frame, "Processed Objects: " + to_string((int)woManager.getProcessedObjects().size()), 55, 10, Scalar(0, 255, 0));
        moveWindow("video", 0, 20);
        imshow("video", current_frame);
        moveWindow("difference", 0, current_frame.rows + 40);
        imshow("difference", medianDifference);
        cvWaitKey(1);
    }
    // updates the world object manager if the video ends while there were still contours.
    if((int)woManager.getCurrentObjects().size() > 0){
        vector<vector<Point>> empty;
        woManager.update(empty, current_frame.clone(), (int)video.get(CV_CAP_PROP_FRAME_COUNT));
    }
    cout << woManager.processedObjectsToString();
    return 0;
}

void setup_ground_truth_events(){
    // ground truth one
    Point top_left = Point(VIDEO_ONE_OBJECT_TOP_LEFT_X, VIDEO_ONE_OBJECT_TOP_LEFT_Y);
    Point bottom_right = Point(VIDEO_ONE_OBJECT_BOTTOM_RIGHT_X, VIDEO_ONE_OBJECT_BOTTOM_RIGHT_Y);
    VideoEvent eventOne = *new VideoEvent(VIDEO_ONE_OBJECT_STATIC, EVENT_ABANDONED, Rect(top_left, bottom_right));
    VideoEvent eventTwo = *new VideoEvent(VIDEO_ONE_OBJECT_REMOVED, EVENT_REMOVED, Rect(top_left, bottom_right));
    ground_truth[VIDEO_ONE_INDEX].addEvent(eventOne);
    ground_truth[VIDEO_ONE_INDEX].addEvent(eventTwo);
    // ground truth two
    top_left = Point(VIDEO_TWO_OBJECT_TOP_LEFT_X, VIDEO_TWO_OBJECT_TOP_LEFT_Y);
    bottom_right = Point(VIDEO_TWO_OBJECT_BOTTOM_RIGHT_X, VIDEO_TWO_OBJECT_BOTTOM_RIGHT_Y);
    eventOne = *new VideoEvent(VIDEO_TWO_OBJECT_STATIC, EVENT_ABANDONED, Rect(top_left, bottom_right));
    eventTwo = *new VideoEvent(VIDEO_TWO_OBJECT_REMOVED, EVENT_REMOVED, Rect(top_left, bottom_right));
    ground_truth[VIDEO_TWO_INDEX].addEvent(eventOne);
    ground_truth[VIDEO_TWO_INDEX].addEvent(eventTwo);
}

void compute_metrics(WorldObjectManager &observed_truth, SystemPerf &ground_truth){
    // compute basic metric
    int groundTruthEvents = (int)ground_truth.getEvents().size();
    int observedTruthEvents = (int)observed_truth.getProcessedObjects().size();
    int groundTruthObservedEvents[groundTruthEvents];
    fill_n(groundTruthObservedEvents, groundTruthEvents, -1);
    int lastObservedEvent = 0;
    for(int j = 0; j < groundTruthEvents; j++){
        bool foundMatch = false;
        for(; lastObservedEvent < observedTruthEvents && !foundMatch; lastObservedEvent++){
            float overlap = ground_truth.getEvents()[j].setOverlap(observed_truth.getProcessedObjects()[lastObservedEvent].getRectRoi());
            if(overlap){
                groundTruthObservedEvents[j] = lastObservedEvent;
                ground_truth.basicMetric.truePositives++;
                foundMatch = true;
            }
        }
        if(!foundMatch)
            ground_truth.basicMetric.falseNegatives++;
    }
    int lastGroundTruth = 0;
    for(int j = 0; j < observedTruthEvents; j++){
        for(; lastGroundTruth < groundTruthEvents; lastGroundTruth++){
            if(groundTruthObservedEvents[lastGroundTruth] == j){
                lastGroundTruth++;
                break;
            }
            ground_truth.basicMetric.falseNegatives++;
        }
    }
    cout << endl << "Basic Metric:" << endl << ground_truth.basicMetric.toString();
    
    // compute dice coefficient
    for(int i = 0; i < groundTruthEvents; i++){
        if(groundTruthObservedEvents[i] != -1){
            vector<float> diceCoeffVector;
            vector<Rect> areaVector = observed_truth.getProcessedObjects()[groundTruthObservedEvents[i]].getRoiVector();
            for(int j = 0; j < (int) areaVector.size(); j++){
                float numerator = (float)(2 * getOverlapArea(ground_truth.getEvents()[i].getRoi(), areaVector[j]));
                float denominator = (float)areaVector[j].area() + (float)ground_truth.getEvents()[i].getRoi().area();
                float diceCoef = numerator / denominator;
                diceCoeffVector.push_back(diceCoef);
            }
            int medianIndex = (int) diceCoeffVector.size()/2;
            ground_truth.events[i].medianDiceCoefficient = diceCoeffVector[medianIndex];
            ground_truth.averageDiceCoefficient += ground_truth.events[i].medianDiceCoefficient;
            cout << "Dice coefficient event #" << i + 1 << " : " << ground_truth.getEvents()[i].medianDiceCoefficient << endl;
        }
    }
    // get average dice coefficient
    ground_truth.averageDiceCoefficient =  (float)(ground_truth.averageDiceCoefficient / (int)observed_truth.getProcessedObjects().size());
    cout << "Average Dice Coefficient: " << ground_truth.averageDiceCoefficient << endl;
    
    // compute time to event
    for(int i = 0; i < groundTruthEvents; i++){
        if(groundTruthObservedEvents[i] != -1){
            int frameObjectAppeared = observed_truth.getProcessedObjects()[groundTruthObservedEvents[i]].getFrameAppeared();
            int frameEvent = ground_truth.events[i].getFrameIndex();
            int frameDiff = frameObjectAppeared - frameEvent;
            float time = (float)(1.0/(float)ground_truth.frameRate) * (float)frameDiff;
            ground_truth.events[i].timeToEvent = time;
        }
        cout << "Time to event #" << i+1 << ": "<< ground_truth.events[i].timeToEvent << endl;
    }
    
    // compute recognition of event metrics
    ground_truth.eventMetric.truePositives = ground_truth.basicMetric.truePositives;
    ground_truth.eventMetric.trueNegatives = ground_truth.basicMetric.trueNegatives;
    ground_truth.eventMetric.falsePositives = ground_truth.basicMetric.falsePositives;
    ground_truth.eventMetric.falseNegatives = ground_truth.basicMetric.falseNegatives;
    for(int i = 0; i < groundTruthEvents; i++){
        if(groundTruthObservedEvents[i] != -1){
            WorldObject obj = observed_truth.getProcessedObjects()[groundTruthObservedEvents[i]];
            if(obj.type == OBJ_ABANDONED && ground_truth.events[i].getEventType() == EVENT_ABANDONED){
                // ok
            }else if(obj.type == OBJ_REMOVED && ground_truth.events[i].getEventType() == EVENT_REMOVED){
                // ok
            }else{
                ground_truth.eventMetric.falseNegatives++;
                ground_truth.eventMetric.truePositives--;
            }
        }
    }
    cout << "Event Metric:" << endl << ground_truth.eventMetric.toString();
}
