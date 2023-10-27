struct ObjectDetectionItem {
    uint8_t index;
    const char* objectName;
    uint8_t filter;
};

// List of objects the pre-trained model is capable of recognizing
// Index number is fixed and hard-coded from training
// Set the filter value to 0 to ignore any recognized objects
ObjectDetectionItem itemList[80] = {
{0,  "mask_weared_incorrect",         1},
{1,  "with_mask",         1},
{2,  "without_mask",         1}};