#pragma once

#include <vector>

#define DL_CLIP(x,low,high) ((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x))

typedef struct {
    int category;              /*!< category index */
    float score;               /*!< score of box */
    std::vector<int> box;      /*!< [left_up_x, left_up_y, right_down_x, right_down_y] */
    std::vector<int> keypoint; /*!< [x1, y1, x2, y2, ...] */
        void limit_box(int width, int height)
    {
        box[0] = DL_CLIP(box[0], 0, width - 1);
        box[1] = DL_CLIP(box[1], 0, height - 1);
        box[2] = DL_CLIP(box[2], 0, width - 1);
        box[3] = DL_CLIP(box[3], 0, height - 1);
    }
    void limit_keypoint(int width, int height)
    {
        for (int i = 0; i < keypoint.size(); i++) {
            if (i % 2 == 0)
                keypoint[i] = DL_CLIP(keypoint[i], 0, width - 1);
            else
                keypoint[i] = DL_CLIP(keypoint[i], 0, height - 1);
        }
    }
    int box_area() const { return (box[2] - box[0]) * (box[3] - box[1]); }
} face_event_t;