//
// Created by 何宇 on 2018/10/1.
//

#ifndef TERRIERSTEIN_SPIDER_H
#define TERRIERSTEIN_SPIDER_H

#include "Scene.hpp"

struct Spider {
    Spider(){}
    Spider(Scene::Transform* t)  {
        this->transform = t;
    }

    void update(float elapsed);

    Scene::Transform* transform;
    bool forward = 0;
    float distance = 0;
    float max_distance = 10.0f;
};


#endif //TERRIERSTEIN_SPIDER_H
