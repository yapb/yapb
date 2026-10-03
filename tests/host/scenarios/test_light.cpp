//
// YaPB test host: unit/engine_light (LightMeasure).
//
// SPDX-License-Identifier: Unlicense
//
// Full light chain on a synthetic single-node BSP: style init/animation,
// surface lightmap sampling and sky color. Modern gamedll staging: the
// Legacy shortcut in getLightLevel must not trigger.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("unit/engine_light [modern]") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "-modern/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_GAMEDIR "-modern/cstrike/dlls/" YAPB_TEST_CSSUFFIX, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  // GameRef sky cvars resolve through the precache pass
  game.Precache ();

  // single node over a single 16x16 surface, plane z = 0 facing up
  mplane_t plane {};
  plane.normal = ystl::Vector (0.0f, 0.0f, 1.0f);
  plane.dist = 0.0f;

  mnode_t leaf {};
  leaf.contents = -1;

  mnode_t root {};
  root.contents = 0;
  root.plane = &plane;
  root.children[0] = &leaf;
  root.children[1] = &leaf;
  root.firstsurface = 0;
  root.numsurfaces = 1;

  mtexinfo_t texinfo {};
  texinfo.vecs[0][0] = 1.0f / 16.0f;
  texinfo.vecs[0][3] = 8.0f;
  texinfo.vecs[1][1] = 1.0f / 16.0f;
  texinfo.vecs[1][3] = 8.0f;

  color24 bright { 255, 255, 255 };
  color24 gray { 8, 8, 8 };
  color24 lightdata { 255, 255, 255 };

  msurface_t surf {};
  surf.flags = 0;
  surf.texturemins[0] = 0;
  surf.texturemins[1] = 0;
  surf.extents[0] = 16;
  surf.extents[1] = 16;
  surf.texinfo = &texinfo;
  surf.styles[0] = 0;
  surf.styles[1] = 255;
  surf.styles[2] = 255;
  surf.styles[3] = 255;
  surf.samples = &bright;

  model_t model {};
  model.nodes = &root;
  model.numnodes = 1;
  model.surfaces = &surf;
  model.numsurfaces = 1;
  model.lightdata = &lightdata;

  const ystl::Vector probe (0.0f, 0.0f, 100.0f);

  // no world model, no light
  illum.ResetWorldModel ();
  CHECK (illum.GetLightLevel (probe) == kInvalidLightLevel);

  // world without lightdata reports full brightness
  model.lightdata = nullptr;
  illum.SetWorldModel (&model);
  CHECK (illum.GetWorldModel () == &model);
  CHECK (illum.GetLightLevel (probe) == 255.0f);
  model.lightdata = &lightdata;

  // default styles (init value 264) saturate a bright sample
  illum.EnableAnimation (true);
  illum.InitializeLightstyles ();
  CHECK (illum.GetLightLevel (probe) == 100.0f);

  // mid-gray sample stays proportional: 8 * 264 >> 8 = 8
  surf.samples = &gray;
  const float dim = illum.GetLightLevel (probe);
  CHECK (dim > 32.5f && dim < 32.8f);
  surf.samples = &bright;

  // dark style 'a' kills the light, 'm' restores the init value
  char dark[2] = { 'a', 0 };
  illum.UpdateLight (0, dark);
  illum.AnimateLight ();
  CHECK (illum.GetLightLevel (probe) == 0.0f);

  char mid[2] = { 'm', 0 };
  illum.UpdateLight (0, mid);
  illum.AnimateLight ();
  CHECK (illum.GetLightLevel (probe) == 100.0f);

  // empty style map falls back to MAX_LIGHTSTYLEVALUE
  char empty[1] = { 0 };
  illum.UpdateLight (0, empty);
  illum.AnimateLight ();
  CHECK (illum.GetLightLevel (probe) == 100.0f);

  // out-of-range styles are ignored, no crash
  char any[2] = { 'a', 0 };
  illum.UpdateLight (99, any);
  illum.AnimateLight ();
  CHECK (illum.GetLightLevel (probe) == 100.0f);

  // with animation disabled the style map is left alone
  illum.EnableAnimation (false);
  illum.UpdateLight (0, dark);
  illum.AnimateLight ();
  CHECK (illum.GetLightLevel (probe) == 100.0f);
  illum.EnableAnimation (true);

  illum.ResetWorldModel ();
  CHECK (illum.GetWorldModel () == nullptr);

  // sky color averages the three sky cvars
  CHECK (illum.GetSkyColor () == 0.0f);

  engine.SetCvar ("sv_skycolor_r", "255");
  CHECK (illum.GetSkyColor () == 85.0f);

  engine.SetCvar ("sv_skycolor_g", "255");
  engine.SetCvar ("sv_skycolor_b", "255");
  CHECK (illum.GetSkyColor () == 255.0f);

  testhost::CloseFakeCs (cs);
}

} // namespace bot
