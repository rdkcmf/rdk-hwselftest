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

#ifndef _HWST_SCENARIO_
#define _HWST_SCENARIO_

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace hwst {

class Comm;
class Diag;
class Sched;
class DiagPrevResults;

struct dependency_t
{
    int e;//index int elements
    bool strict; //marks that if this dependency fails the component should also fail immediately
};

struct element_t
{
    std::shared_ptr<Diag> diag;
    std::vector<dependency_t> dependencies;
};

struct group_t
{
    std::vector<std::weak_ptr<Diag>> diags;
};

class Scenario
{
    friend hwst::Sched;
    friend DiagPrevResults;

public:
    Scenario();
    virtual ~Scenario();
    virtual bool init(std::string options) = 0;

protected:
    int addElement(std::unique_ptr<Diag> d);//-1: not added, >=0: added at position
    bool addDependency(int e, int dep, bool strict=false);

private:
    std::mutex apiMutex;
    std::recursive_mutex elementsMutex;
    std::vector<element_t> elements;
    std::vector<group_t> groups;
    int checkDependencies(int e);//-1: stop with error, 0:wait, 1:dependencies satisfied
    int nextToRun(std::shared_ptr<Diag> &d);//-1: no more to run, 0:some to run but not ready yet, 1-element run
    bool checkAllDone();
};

} // namespace hwst

#endif // _HWST_SCENARIO_
