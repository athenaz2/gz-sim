/*
 * Copyright (C) 2022 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gtest/gtest.h>

#include <gz/msgs/image.pb.h>

#include <string>

#include <gz/transport/Node.hh>
#include <gz/utils/ExtraTestMacros.hh>

#include "gz/sim/Server.hh"
#include "gz/sim/SystemLoader.hh"
#include "gz/sim/Util.hh"
#include "test_config.hh"

#include "../helpers/EnvTestFixture.hh"

using namespace gz;
using namespace std::chrono_literals;

std::mutex mutex;
int cbCount = 0;
int giEnabled = false;

//////////////////////////////////////////////////
/// Note: This test is almost identical to the test in
/// camera_sensor_scene_background.cc, and the `cameraCb` could have been
/// reused, but loading the world twice in a single processes causes errors with
/// Ogre.
class CameraSensorGlobalIlluminationTest :
  public InternalFixture<InternalFixture<::testing::Test>>
{
};

/////////////////////////////////////////////////
void cameraCb(const msgs::Image & _msg)
{
  ASSERT_EQ(msgs::PixelFormatType::RGB_INT8,
      _msg.pixel_format_type());

  for (unsigned int y = 0; y < _msg.height(); ++y)
  {
    for (unsigned int x = 0; x < _msg.width(); ++x)
    {
      unsigned char r = _msg.data()[y * _msg.step() + x*3];
      if (!giEnabled)
        ASSERT_LT(static_cast<int>(r), 10);
      else
        ASSERT_GT(static_cast<int>(r), 10);

      unsigned char g = _msg.data()[y * _msg.step() + x*3+1];
      ASSERT_EQ(0, static_cast<int>(g));

      unsigned char b = _msg.data()[y * _msg.step() + x*3+2];
      ASSERT_EQ(0, static_cast<int>(b));
    }
  }
  std::lock_guard<std::mutex> lock(mutex);
  if (!giEnabled)
    cbCount = 1;
  else
    cbCount = 2;
}

/////////////////////////////////////////////////
// Check that sensor reads a very dark value when GI is not enabled
TEST_F(CameraSensorGlobalIlluminationTest,
       GlobalIlluminationNotEnabled)
{
  const auto sdfFile = common::joinPaths(std::string(PROJECT_SOURCE_PATH),
    "test", "worlds", "camera_sensor_gi_enabled_false.sdf");
  // Start server
  sim::ServerConfig serverConfig;
  serverConfig.SetSdfFile(sdfFile);

  sim::Server server(serverConfig);
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));

  // subscribe to the camera topic
  transport::Node node;
  cbCount = 0;
  giEnabled = false;
  node.Subscribe("/camera", &cameraCb);

  // Run server and verify that we are receiving a message
  // from the depth camera
  server.Run(true, 100, false);
  
  int i = 0;
  while (i < 100 && cbCount == 0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    i++;
  }

  std::lock_guard<std::mutex> lock(mutex);
  EXPECT_EQ(cbCount, 1);
}

/////////////////////////////////////////////////
// Check that sensor reads less dark value when GI is enabled
TEST_F(CameraSensorGlobalIlluminationTest,
       GlobalIlluminationEnabled)
{
  const auto sdfFile = common::joinPaths(std::string(PROJECT_SOURCE_PATH),
    "test", "worlds", "camera_sensor_gi_enabled_true.sdf");
  // Start server
  sim::ServerConfig serverConfig;
  serverConfig.SetSdfFile(sdfFile);

  sim::Server server(serverConfig);
  EXPECT_FALSE(server.Running());
  EXPECT_FALSE(*server.Running(0));

  // subscribe to the camera topic
  transport::Node node;
  cbCount = 1;
  giEnabled = true;
  node.Subscribe("/camera", &cameraCb);

  // Run server and verify that we are receiving a message
  // from the depth camera
  server.Run(true, 100, false);

  int i = 0;
  while (i < 100 && cbCount == 0)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    i++;
  }

  std::lock_guard<std::mutex> lock(mutex);
  EXPECT_EQ(cbCount, 2);
}