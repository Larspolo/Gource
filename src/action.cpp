/*
    Copyright (C) 2009 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "action.h"

int getLen(RCommit commit, RFile* file) {
    std::string gitCmd = "git -C \"" + gGourceSettings.path + "\" show " + commit.hash + ":" + file->fullpath.substr(1) + " > temp2.txt";

    std::system(gitCmd.c_str());

    std::ifstream testFile("temp2.txt");
    std::string rline;
    int state = 0;
    int found = 0;
    while(getline(testFile, rline)){
        // Get length of duplicate code
        if (state == 0) {
            found += atoi(rline.substr(0, rline.find(" ")).c_str());

            state = 1;
        }
        // Get files that duplicate code is in
        else if (state == 1) {
            // Go to next state if no more files
            if (rline[0] == '=') {
                state = 2;
            }
        }
        // Loop over duplicate code
        else if (state == 2) {
            // End duplicate code, and go to next section
            if (rline[0] == '=') {
                state = 0;
            }
        }
    }

    return found;
}

int lenFromColor(vec3 color) {
    if (color[0] == 1.0f)
        return 100;
    if (color[2] == .2f)
        return 0;
    if (color[0] != .0f)
        return (color[0] - .2) / .8 * 100.0;
    else
        return -((color[1] - .2) / .8 * 100.0);
}

vec3 colorFromLen(int len) {
    if (len > 100)
        return vec3(1.0f, .0f, .0f);
    if (len == 0)
        return vec3(.2f);
    if (len > 0)
        return vec3((float)(len / 100.0 *.8 + .2), .0f, .0f);
    else
        return vec3(.0f, (float)(len / 100.0 *.8 + .2), .0f);
}

RAction::RAction(RUser* source, RFile* target, time_t timestamp, float t, const vec3& colour)
    : colour(colour), source(source), target(target), timestamp(timestamp), t(t), progress(0.0f), rate(0.5f) {
}

void RAction::apply(RCommit commit) {
    target->touch(timestamp, colour);
}

void RAction::logic(float dt, RCommit currentCommit) {
    if(progress >= 1.0) return;

    if(progress == 0.0) {
        apply(currentCommit);
    }

    float action_rate = std::min(10.0f, rate * std::max(1.0f, ((float)source->getPendingActionCount())));

    progress = std::min(progress + action_rate * dt, 1.0f);
}

void RAction::drawToVBO(quadbuf& buffer) const {
    if(isFinished()) return;

    vec2 src  = source->getPos();
    vec2 dest = target->getAbsolutePos();

    //TODO: could use glm::perp

    vec2 n    = normalise(dest - src);
    vec2 perp = vec2(-n.y, n.x);

    vec2 offset     = perp * target->getSize() * 0.5f;
    vec2 offset_src = offset * 0.3f;

    float alpha = 1.0 - progress;
    float alpha2 = alpha * 0.1;

    vec4 col1 = vec4(colour, alpha);
    vec4 col2 = vec4(colour, alpha2);

    quadbuf_vertex v1(src  - offset_src,  col2, vec2(0.0f, 0.0f));
    quadbuf_vertex v2(src  + offset_src,  col2, vec2(0.0f, 1.0f));
    quadbuf_vertex v3(dest + offset,      col1, vec2(1.0f, 1.0f));
    quadbuf_vertex v4(dest - offset,      col1, vec2(1.0f, 0.0f));

    buffer.add(0, v1, v2, v3, v4);
}

void RAction::draw(float dt) {
    if(isFinished()) return;

    vec2 src  = source->getPos();
    vec2 dest = target->getAbsolutePos();

    vec2 n    = normalise(dest - src);
    vec2 perp = vec2(-n.y, n.x);

    vec2 offset     = perp * target->getSize() * 0.5f;
    vec2 offset_src = offset * 0.3f;

    float alpha = 1.0 - progress;
    float alpha2 = alpha * 0.1;

    vec4 col1 = vec4(colour, alpha);
    vec4 col2 = vec4(colour, alpha2);

    glBegin(GL_QUADS);
        glColor4fv(glm::value_ptr(col2));
        glTexCoord2f(0.0,0.0);
        glVertex2f(src.x - offset_src.x, src.y - offset_src.y);
        glTexCoord2f(0.0,1.0);
        glVertex2f(src.x + offset_src.x, src.y + offset_src.y);

        glColor4fv(glm::value_ptr(col1));
        glTexCoord2f(1.0,1.0);
        glVertex2f(dest.x + offset.x, dest.y + offset.y);
        glTexCoord2f(1.0,0.0);
       glVertex2f(dest.x - offset.x, dest.y - offset.y);
    glEnd();
}

CreateAction::CreateAction(RUser* source, RFile* target, time_t timestamp, float t)
    : RAction(source, target, timestamp, t, target->getColour()) {
}

RemoveAction::RemoveAction(RUser* source, RFile* target, time_t timestamp, float t)
    : RAction(source, target, timestamp, t, target->getColour()) {
}

void RemoveAction::logic(float dt, RCommit currentCommit) {
    float old_progress = progress;

    RAction::logic(dt, currentCommit);

    if(old_progress < 1.0 && progress >= 1.0) {
        target->remove(timestamp);
    }
}

ModifyAction::ModifyAction(RUser* source, RFile* target, time_t timestamp, float t, const vec3& modify_colour)
    : RAction(source, target, timestamp, t, target->getColour()), modify_colour(modify_colour) {
}

void ModifyAction::apply(RCommit commit) {
    RAction::apply(commit);

    int len = getLen(commit, target);
    vec3 color = colorFromLen(len);

    target->setFileColour(color);
}
