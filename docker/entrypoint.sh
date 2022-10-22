#!/bin/bash

echo -e "\e[1;33m=====================================================================\e[0m"
echo -e "\e[1;33m▀█▄   ▀█▀                   ██                    ▄                   \e[0m"
echo -e "\e[1;33m █▀█   █   ▄▄▄▄   ▄▄▄▄ ▄▄▄ ▄▄▄    ▄▄▄ ▄  ▄▄▄▄   ▄██▄    ▄▄▄   ▄▄▄ ▄▄  \e[0m"
echo -e "\e[1;33m █ ▀█▄ █  ▀▀ ▄██   ▀█▄  █   ██   ██ ██  ▀▀ ▄██   ██   ▄█  ▀█▄  ██▀ ▀▀ \e[0m"
echo -e "\e[1;33m █   ███  ▄█▀ ██    ▀█▄█    ██    █▀▀   ▄█▀ ██   ██   ██   ██  ██     \e[0m"
echo -e "\e[1;33m▄█▄   ▀█  ▀█▄▄▀█▀    ▀█    ▄██▄  ▀████▄ ▀█▄▄▀█▀  ▀█▄▀  ▀█▄▄█▀ ▄██▄    \e[0m"
echo -e "\e[1;33m                                ▄█▄▄▄▄▀                               \e[0m"
echo -e "\e[1;33m=====================================================================\e[0m"    
echo "Developed by Nova, a student-run autonomous driving group at UT Dallas"
echo "Find out more at https://nova-utd.github.io/navigator"   
echo "🦊 Sourcing ROS2 Foxy..."
source /opt/ros/foxy/setup.bash

echo "🧭 Sourcing Navigator..."

echo "🔌 Setting up CARLA API..."
export CARLA_ROOT="/workspace/simulator"
export SCENARIO_RUNNER_ROOT="/workspace/scenario_runner"
export LEADERBOARD_ROOT="/workspace/leaderboard"
export PYTHONPATH="${CARLA_ROOT}/PythonAPI/carla/":"${SCENARIO_RUNNER_ROOT}":"${LEADERBOARD_ROOT}":"${CARLA_ROOT}/PythonAPI/carla/dist/carla-0.9.13-py3.7-linux-x86_64.egg":${PYTHONPATH}
# export PYTHONPATH="/navigator/data/carla-0.9.13-py3.7-linux-x86_64.egg":${PYTHONPATH}

/bin/bash
