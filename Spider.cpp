//
// Created by 何宇 on 2018/10/1.
//

#include "Spider.h"

void Spider::update(float elapsed) {
    glm::mat3 directions = glm::mat3_cast(transform->rotation);
    float amt = 2.0f * elapsed;

    glm::vec3 step = glm::vec3(0.0f);

    step = amt * directions[1];

    transform->position.x += step.x;
    transform->position.y += step.y;
    transform->position.z += step.z;

    distance += glm::length(step);

    if (distance >= max_distance) {
        distance = 0;
        forward = !forward;
        transform->rotation *= glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    }
}

