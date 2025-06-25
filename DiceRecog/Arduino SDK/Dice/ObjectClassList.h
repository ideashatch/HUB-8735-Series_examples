#ifndef __OBJECTCLASSLIST_H__
#define __OBJECTCLASSLIST_H__

struct ObjectDetectionItem {
    uint8_t index;
    const char* objectName;
    uint8_t filter;
};

// List of objects the pre-trained model is capable of recognizing
// Index number is fixed and hard-coded from training
// Set the filter value to 0 to ignore any recognized objects
ObjectDetectionItem itemList[6] = {
    {0,  "1",         1},
    {1,  "2",        1},
    {2,  "3",            1},
    {3,  "4",      1},
    {4,  "5",      1},
    {5,  "6",            1},
  };
#endif
