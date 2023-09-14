struct ObjectDetectionItem {
    uint8_t index;
    const char* objectName;
    uint8_t filter;
};

// List of objects the pre-trained model is capable of recognizing
// Index number is fixed and hard-coded from training
// Set the filter value to 0 to ignore any recognized objects
ObjectDetectionItem itemList[10] = {
{0,  "0",          1},
{1,  "1",         1},
{2,  "2",             1},
{3,  "3",       1},
{4,  "4",       1},
{5,  "5",             1},
{6,  "6",           1},
{7,  "7",           1},
{8,  "8",            1},
{9, "9",      1}};
