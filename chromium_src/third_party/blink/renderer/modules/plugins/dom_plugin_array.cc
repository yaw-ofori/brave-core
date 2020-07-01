/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/brave_farbling_constants.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include <random>

#define BRAVE_DOM_PLUGINS_UPDATE_PLUGIN_DATA                                   \
  if (GetFrame() && GetFrame()->GetContentSettingsClient()) {                  \
    switch (GetFrame()->GetContentSettingsClient()->GetBraveFarblingLevel()) { \
      case BraveFarblingLevel::OFF: {                                          \
        break;                                                                 \
      }                                                                        \
      case BraveFarblingLevel::MAXIMUM: {                                      \
        dom_plugins_.clear();                                                  \
        U_FALLTHROUGH;                                                         \
      }                                                                        \
      case BraveFarblingLevel::BALANCED: {                                     \
        for (unsigned index = 0; index < length(); index++) {                  \
          dom_plugins_[index] = item(index);                                   \
        }                                                                      \
        auto* fake_plugin_info_1 = MakeGarbageCollected<PluginInfo>(           \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .GenerateRandomString("PLUGIN_1_NAME", 8),                     \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .GenerateRandomString("PLUGIN_1_FILENAME", 16),                \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .GenerateRandomString("PLUGIN_1_DESCRIPTION", 32),             \
            0, false);                                                         \
        auto* fake_dom_plugin_1 =                                              \
            MakeGarbageCollected<DOMPlugin>(GetFrame(), *fake_plugin_info_1);  \
        dom_plugins_.push_back(fake_dom_plugin_1);                             \
        auto* fake_plugin_info_2 = MakeGarbageCollected<PluginInfo>(           \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .GenerateRandomString("PLUGIN_2_NAME", 7),                     \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .GenerateRandomString("PLUGIN_2_FILENAME", 15),                \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .GenerateRandomString("PLUGIN_2_DESCRIPTION", 31),             \
            0, false);                                                         \
        auto* fake_dom_plugin_2 =                                              \
            MakeGarbageCollected<DOMPlugin>(GetFrame(), *fake_plugin_info_2);  \
        dom_plugins_.push_back(fake_dom_plugin_2);                             \
        std::mt19937_64 prng =                                                 \
            brave::BraveSessionCache::From(*(GetFrame()->GetDocument()))       \
                .MakePseudoRandomGenerator();                                  \
        std::shuffle(dom_plugins_.begin(), dom_plugins_.end(), prng);          \
        break;                                                                 \
      }                                                                        \
      default:                                                                 \
        NOTREACHED();                                                          \
    }                                                                          \
  }

#include "../../../../../../third_party/blink/renderer/modules/plugins/dom_plugin_array.cc"

#undef BRAVE_DOM_PLUGIN_ARRAY_GET_PLUGIN_DATA
