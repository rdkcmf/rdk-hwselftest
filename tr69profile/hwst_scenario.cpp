/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <algorithm>
#include <utility>

#include "hwst_diag.hpp"
#include "hwst_diagfactory.hpp"
#include "hwst_scenario.hpp"

//#define HWST_DEBUG 1
#ifdef HWST_DEBUG
#include <unistd.h>
#define HWST_DBG(str) do {std::cout << "HWST_DBG |" << str << std::endl;} while(false)
#else
#define HWST_DBG(str) do {} while(false)
#endif

using hwst::Diag;

namespace hwst {

Scenario::Scenario()
{
};

Scenario::~Scenario()
{
    HWST_DBG("~Scenario");
};

int Scenario::addElement(std::unique_ptr<Diag> d)
{
    std::lock_guard<std::recursive_mutex> elementsLock(elementsMutex);
    for(auto const& e: elements)
    {
        if(*(e.diag) == *d)
        {
            HWST_DBG("equal");
            return -1;
        }
    }

    int s = elements.size();
    elements.push_back(element_t({std::shared_ptr<Diag>(std::move(d))}));
    if(elements.size() == s)
         return -1;

    elements[s].diag->setEnabled();
    return s;
}

int Scenario::getElement(const std::string& elem_name)
{
    for (int i = 0; i < elements.size(); i++)
        if (elements[i].diag->getName() == elem_name)
            return i;

    return -1;
}

/** @note: No check against cross-dependancy! */
bool Scenario::addDependency(int e, int dep, bool strict)
{
    std::lock_guard<std::recursive_mutex> elementsLock(elementsMutex);

    int s = elements[e].dependencies.size();
    elements[e].dependencies.push_back(dependency_t({dep, strict}));
    if(elements[e].dependencies.size() == s)
         return false;

    HWST_DBG("Scenario: addDependency: " + std::to_string(e) + "=" + elements[e].diag->name +
        "[" + std::to_string(dep) + "=" + elements[dep].diag->name + "]");

    return true;
}

int Scenario::checkDependencies(int e)
{
    int status = 1;
    std::lock_guard<std::recursive_mutex> elementsLock(elementsMutex);

    for(auto const& dep: elements[e].dependencies)
    {
        switch((elements[dep.e].diag->getStatus()).state)
        {
        case Diag::issued:
        case Diag::running:
        case Diag::enabled:
            status = 0;
            /* some are still not finished yet */
            goto end;
        case Diag::error:
            if(dep.strict)
            {
                HWST_DBG("Scenario::checkDependencies:-1");
                status = -1;
                goto end;
            }
        case Diag::disabled:
        case Diag::finished:
        default:
            /* finished or not to be run - continue all the others */
            break;
        }
    }

end:
    HWST_DBG("Scenario::checkDependencies:" + std::to_string(status));
    return status;
}

int Scenario::nextToRun(std::shared_ptr<Diag> &diag)
{
    std::lock_guard<std::recursive_mutex> elementsLock(elementsMutex);
    int status = -1;

    for(auto const& e: elements)
    {
        HWST_DBG("Scenario: nextToRun at " + std::to_string(&e - &elements[0]) + ":" + e.diag->name);
        switch(e.diag->getStatus().state)
        {
            case Diag::enabled:
                switch(checkDependencies(&e - &elements[0]))
                {
                case 1:
                    /* ready to run */
                    status = 1;
                    diag = e.diag;
                    HWST_DBG("Scenario: found ready:" + diag->name);
                    goto end;

                case 0:
                    status = 0;
                    HWST_DBG("Scenario: pending:" + e.diag->name);
                    /* pending for dependency, skip and try others */
                    break;

                case -1:
                default:
                    /* strict dependency failed - force error, mark finished, try others */
                    e.diag->setFinished(Diag::INTERNAL_TEST_ERROR, "Dependency failed.");
                    break;

                }
                break;

            case Diag::issued:
            case Diag::running:
                /* in progress */
                /* no break */
            case Diag::error:
            case Diag::disabled:
            case Diag::finished:
            default:
                /* finished */
                /* skip and try others */
                break;
        }
    }
end:
    return status;
}

bool Scenario::checkAllDone()
{
    std::lock_guard<std::recursive_mutex> elementsLock(elementsMutex);
    int status = true;

    for(auto const& e: elements)
    {
        HWST_DBG("Scenario: allDone at " + std::to_string(&e - &elements[0]) + ":" + e.diag->name);
        switch(e.diag->getStatus().state)
        {
            case Diag::enabled:
            case Diag::issued:
            case Diag::running:
                /* in progress */
                status = false;
                goto end;
            case Diag::error:
            case Diag::disabled:
            case Diag::finished:
            default:
                //state = finished;
                /* finished */
                break;
        }
    }
end:
    return status;
}

} // namespace hwst
