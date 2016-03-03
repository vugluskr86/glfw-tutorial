/********************************************************
 * Engine is our container for our main program logic
 * 
 * This allows us to only place code in main.c that
 * interacts with the user through our framework and
 * thus change framework if needed.
 * 
 ********************************************************/

#include "engine.h"

//////////////////////////////////////////////////////////
// For now we just some global state to make life easy

EngineKeyPressed engineKeyPressedCallback = NULL;

// info about what is supported, this should move into our shader library at some point
int           maxPatches = 0; // how many patches can our shader output
int           maxTessLevel = 0; // what is the maximum tesselation level we support

// object info
llist *       materials = NULL;
meshNode *    scene = NULL;
meshNode *    tieNodes[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

// and some globals for our fonts
FONScontext * fs = NULL;
int           font = FONS_INVALID;
float         lineHeight = 0.0f;

// shaders
shaderInfo *  skyboxShader = NULL;
shaderInfo *  hmapShader = NULL;
shaderInfo *  colorShader = NULL;
shaderInfo *  texturedShader = NULL;
shaderInfo *  reflectShader = NULL;

// lights
lightSource   sun;

// our camera
mat4          view;
vec3          camera_eye = { 0.0, 1000.0, 1300.0 };
vec3          camera_lookat =  { 0.0, 1000.0, 0.0 };

// and some runtime variables.
bool          wireframe = false;
double        frames = 0.0f;
double        fps = 0.0f;
double        lastframes = 0.0f;
double        lastsecs = 0.0f;

//////////////////////////////////////////////////////////
// keyboard handling

// sets a callback that allows us to test if a key is
// pressed
void engineSetKeyPressedCallback(EngineKeyPressed pCallback) {
  engineKeyPressedCallback = pCallback;
};

//////////////////////////////////////////////////////////
// fonts
void load_font() {
  // we start with creating a font context that tells us about the font we'll be rendering
  fs = gl3fonsCreate(512, 512, FONS_ZERO_TOPLEFT);
  if (fs != NULL) {
    // then we load our font
    #ifdef __APPLE__
      font = fonsAddFont(fs, "sans", "Fonts/DroidSerif-Regular.ttf");
    #else
      font = fonsAddFont(fs, "sans", "Resoures\\Fonts\\DroidSerif-Regular.ttf");
    #endif
    if (font != FONS_INVALID) {
      // setup our font
      fonsSetColor(fs, gl3fonsRGBA(255,255,255,255)); // white
      fonsSetSize(fs, 16.0f); // 16 point font
      fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP); // left/top aligned
      fonsVertMetrics(fs, NULL, NULL, &lineHeight);
    } else {
      errorlog(-201, "Couldn't load DroidSerif-Regular.ttf");       
    };
  } else {
    errorlog(-200, "Couldn't create our font context");
  };
};

void unload_font() {
  if (fs != NULL) {
    gl3fonsDelete(fs);
    fs = NULL;
  };
};

//////////////////////////////////////////////////////////
// Shaders

void load_shaders() {
  // init some paths
  #ifdef __APPLE__
    shaderSetPath("Shaders/");
  #else
    shaderSetPath("Resources\\Shaders\\");
  #endif

  // get info about our tesselation capabilities
  glGetIntegerv(GL_MAX_PATCH_VERTICES, &maxPatches);
  errorlog(0, "Supported patches: %d", maxPatches);
  if (maxPatches >= 4) {
    // setup using quads
    glPatchParameteri(GL_PATCH_VERTICES, 4);

    glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &maxTessLevel);
    errorlog(0, "Maximum supported tesselation level: %d", maxTessLevel);
  };

  skyboxShader = newShader("skybox", "skybox.vs", NULL, NULL, NULL, "skybox.fs");

  if (maxPatches >= 4) {
    hmapShader = newShader("hmap", "hmap_ts.vs", "hmap_ts.ts", "hmap_ts.te", "hmap_ts.gs", "hmap_ts.fs");
  } else {
    hmapShader = newShader("hmap", "hmap.vs", NULL, NULL, NULL, "hmap.fs");
  };

  colorShader = newShader("flatcolor", "standard.vs", NULL, NULL, NULL, "flatcolor.fs");
  texturedShader = newShader("textured", "standard.vs", NULL, NULL, NULL, "textured.fs");
  reflectShader = newShader("reflect", "standard.vs", NULL, NULL, NULL, "reflect.fs");
};

void unload_shaders() {
  if (colorShader != NULL) {
    shaderRelease(colorShader);
  };
  if (texturedShader != NULL) {
    shaderRelease(texturedShader);
  };
  if (reflectShader != NULL) {
    shaderRelease(reflectShader);
  };
  if (skyboxShader != NULL) {
    shaderRelease(skyboxShader);    
  };
  if (hmapShader != NULL) {
    shaderRelease(hmapShader);    
  };
};

//////////////////////////////////////////////////////////
// Objects

void initHMap() {
  material *    mat;
  meshNode *    mnode;
  mesh3d *      mesh;

  mat = newMaterial("hmap");                  // create a material for our heightmap
  matSetShader(mat, hmapShader);              // texture shader for now
  matSetDiffuseMap(mat, getTextureMapByFileName("grass.jpg", GL_LINEAR, GL_REPEAT, false));
  matSetBumpMap(mat, getTextureMapByFileName("heightfield.jpg", GL_LINEAR, GL_REPEAT, true));

  mesh = newMesh(102 * 102, 101 * 101 * 3 * 2);
  strcpy(mesh->name, "hmap");
  meshSetMaterial(mesh, mat);
  meshMakePlane(mesh, 101, 101, 101.0, 101.0, maxPatches >= 4);
  meshCopyToGL(mesh, true);

  // now add our heightfield to our scene
  mnode = newMeshNode("hmap");
  meshNodeSetMesh(mnode, mesh);
  meshNodeAddChild(scene, mnode);

  // we can release these seeing its now all contained within our scene
  matRelease(mat);
  meshRelease(mesh);
};

void initSkybox() {
  material *    mat;
  meshNode *    mnode;
  mesh3d *      mesh;

  mat = newMaterial("skybox");                // create a material for our skybox
  matSetShader(mat, skyboxShader);            // use our skybox shader, this will cause our lighting and positioning to be ignored!!!
  matSetDiffuseMap(mat, getTextureMapByFileName("skybox.png", GL_LINEAR, GL_CLAMP_TO_EDGE, false)); // load our texture map (courtesy of http://rbwhitaker.wikidot.com/texture-library)
 
  mesh = newMesh(24, 36);                     // init our cube with enough space for our buffers
  strcpy(mesh->name,"skybox");                // set name to cube
  meshSetMaterial(mesh, mat);                 // assign our material
  meshMakeCube(mesh, 100000.0, 100000.0, 100000.0, true);  // create our cube, we make it as large as possible
  meshFlipFaces(mesh);                        // turn the mesh inside out
  meshCopyToGL(mesh, true);                   // copy our cube data to the GPU

  // now add our skybox to our scene
  mnode = newMeshNode("skybox");
  meshNodeSetMesh(mnode, mesh);
  meshNodeAddChild(scene, mnode);

  // we can release these seeing its now all contained within our scene
  matRelease(mat);
  meshRelease(mesh);
};

void load_objects() {
  char          modelPath[1024];
  char *        text;
  vec3          tmpvector;
  mat4          adjust;
  material *    mat;
  llistNode *   lnode;

  // init some paths
  #ifdef __APPLE__
    tmapSetTexturePath("Textures/");
    strcpy(modelPath,"Models/");
  #else
    tmapSetTexturePath("Resources\\Textures\\");
    strcpy(modelPath,"Resources\\Models\\");
  #endif

  // create a retainer for materials
  materials = newMatList();

  // create our default material, make sure it's the first one
  mat = newMaterial("Default");
  matSetShader(mat, colorShader);
  llistAddTo(materials, mat);
  matRelease(mat);
  mat = NULL;

  // load our mtl file(s)
  text = loadFile(modelPath,"tie-bomber.mtl");
  if (text != NULL) {
    matParseMtl(text, materials);
      
    free(text);
  };    

  // assign shaders to our materials
  lnode = materials->first;
  while (lnode != NULL) {
    mat = (material * ) lnode->data;

    if (mat->reflectMap != NULL) {  
      matSetShader(mat, reflectShader);
    } else if (mat->diffuseMap != NULL) {          
      matSetShader(mat, texturedShader);
    } else {
      matSetShader(mat, colorShader);
    };
    
    lnode = lnode->next;
  };

  // create our root node
  scene = newMeshNode("scene");
  if (scene != NULL) {
    // load our tie-bomber obj file
    text = loadFile(modelPath, "tie-bomber.obj");
    if (text != NULL) {
      llist *       meshes = newMeshList();

      // setup our adjustment matrix to center our object
      mat4Identity(&adjust);
      mat4Translate(&adjust, vec3Set(&tmpvector, 250.0, -100.0, 100.0));

      // parse our object file
      meshParseObj(text, meshes, materials, &adjust);

      // add our tie bomber mesh to our containing node
      tieNodes[0] = newMeshNode("tie-bomber-0");
      mat4Translate(&tieNodes[0]->position, vec3Set(&tmpvector, 0.0, 1000.0, 0.0));
      meshNodeAddChildren(tieNodes[0], meshes);

      // and add it to our scene, note that we could free up our tie-bomber node here as it is references by our scene
      // but we keep it so we can interact with them.
      meshNodeAddChild(scene, tieNodes[0]);

      // and free up what we no longer need
      llistFree(meshes);
      free(text);
    };

    // instance our tie bomber a few times, note that we're copying our tieNodes[0] model matrix as well so all our other bombers will be placed relative to it.
    tieNodes[1] = newCopyMeshNode("tie-bomber-1", tieNodes[0], false);
    mat4Translate(&tieNodes[1]->position, vec3Set(&tmpvector, -400.0, 0.0, -100.0));
    meshNodeAddChild(scene, tieNodes[1]);

    tieNodes[2] = newCopyMeshNode("tie-bomber-2", tieNodes[0], false);
    mat4Translate(&tieNodes[2]->position, vec3Set(&tmpvector, 400.0, 0.0, -100.0));
    meshNodeAddChild(scene, tieNodes[2]);

    tieNodes[3] = newCopyMeshNode("tie-bomber-3", tieNodes[0], false);
    mat4Translate(&tieNodes[3]->position, vec3Set(&tmpvector, 0.0, 0.0, 500.0));
    meshNodeAddChild(scene, tieNodes[3]);

    tieNodes[4] = newCopyMeshNode("tie-bomber-4", tieNodes[0], false);
    mat4Translate(&tieNodes[4]->position, vec3Set(&tmpvector, -600.0, 0.0, -400.0));
    meshNodeAddChild(scene, tieNodes[4]);

    tieNodes[5] = newCopyMeshNode("tie-bomber-5", tieNodes[0], false);
    mat4Translate(&tieNodes[5]->position, vec3Set(&tmpvector, 600.0, 0.0, -400.0));
    meshNodeAddChild(scene, tieNodes[5]);

    tieNodes[6] = newCopyMeshNode("tie-bomber-6", tieNodes[0], false);
    mat4Translate(&tieNodes[6]->position, vec3Set(&tmpvector, 0.0, 0.0, -500.0));
    meshNodeAddChild(scene, tieNodes[6]);

    tieNodes[7] = newCopyMeshNode("tie-bomber-7", tieNodes[0], false);
    mat4Translate(&tieNodes[7]->position, vec3Set(&tmpvector, -800.0, 0.0, -800.0));
    meshNodeAddChild(scene, tieNodes[7]);

    tieNodes[8] = newCopyMeshNode("tie-bomber-8", tieNodes[0], false);
    mat4Translate(&tieNodes[8]->position, vec3Set(&tmpvector, 800.0, 0.0, -800.0));
    meshNodeAddChild(scene, tieNodes[8]);

    tieNodes[9] = newCopyMeshNode("tie-bomber-9", tieNodes[0], false);
    mat4Translate(&tieNodes[9]->position, vec3Set(&tmpvector, 0.0, 0.0, -1000.0));
    meshNodeAddChild(scene, tieNodes[9]);

    // create our height map object
    initHMap();

    // And create our skybox, we no longer add this to our scene so we can handle this last in our rendering loop
    initSkybox();
  }; 
};

void unload_objects() {
  int i;

  // free our object data
  errorlog(0, "Unloading objects...");

  // release our tie-bomber nodes
  for (i = 0; i < 10; i++) {
    if (tieNodes[i] != NULL) {
      meshNodeRelease(tieNodes[i]);
      tieNodes[i] = NULL;
    };
  };

  if (scene != NULL) {
    meshNodeRelease(scene);
    scene = NULL;
  };
  
  if (materials != NULL) {
    llistFree(materials);
    materials = NULL;
  };

  // do this last just in case...
  tmapReleaseCachedTextureMaps();
};

//////////////////////////////////////////////////////////
// Main engine

// engineInit initializes any variables, kinda like our constructor
void engineInit() {

};

// engineLoad loads any data that we need to load before we can start outputting stuff
void engineLoad() {
  vec3  upvector;
  
  // load our font
  load_font();
    
  // load, compile and link our shader(s)
  load_shaders();
  
  // setup our light
  vec3Set(&sun.position, 100000.0, 100000.0, 0.00);
  
  // load our objects
  load_objects();
  
  // init our view matrix
  mat4Identity(&view);
  mat4LookAt(&view, &camera_eye, &camera_lookat, vec3Set(&upvector, 0.0, 1.0, 0.0));  
};

// engineUnload unloads and frees up any data associated with our engine
void engineUnload() {
  // lets be nice and cleanup
  unload_shaders();
  unload_objects();
  unload_font();
};

// engineUpdate is called to handle any updates of our data
// pSecondsPassed is the number of seconds passed since our application was started.
void engineUpdate(double pSecondsPassed) {
  const joystickInfo * joystick = getJoystickInfo(0);
  float         delta;
  vec3          avector, bvector, upvector;
  mat4          M;
  float         moveHorz = 0.0;
  float         moveVert = 0.0;
  float         moveForward = 0.0;
  float         moveSideways = 0.0;
  
  // handle our joystick
  if (joystick == NULL) {
    // no joystick
  } else if (joystick->enabled) {
    moveHorz = joystick->axes[0] * 2.0;
    moveVert = joystick->axes[1] * 2.0;
    moveForward = joystick->axes[3] * 20.0;
    moveSideways = joystick->axes[2] * 20.0;
  };
    
  // handle our keys....
  if (engineKeyPressedCallback(GLFW_KEY_A)) {
    moveHorz = 1.0;
  } else if (engineKeyPressedCallback(GLFW_KEY_D)) {
    moveHorz = -1.0;
  };
  if (engineKeyPressedCallback(GLFW_KEY_W)) {
    moveVert = 1.0;
  } else if (engineKeyPressedCallback(GLFW_KEY_S)) {
    moveVert = -1.0;
  };
  if (engineKeyPressedCallback(GLFW_KEY_Z)) {
    moveForward = -10.0;
  } else if (engineKeyPressedCallback(GLFW_KEY_C)) {
    moveForward = 10.0;
  };
    
  // adjust camera
  if ((moveHorz <= -0.1) || (moveHorz >= 0.1)) {
    // rotate position left
    
    // get our (reverse) looking direction vector
    vec3Copy(&avector, &camera_eye);
    vec3Sub(&avector, &camera_lookat);
    
    // rotate our looking direction vector around our up vector
    mat4Identity(&M);
    mat4Rotate(&M, moveHorz, vec3Set(&bvector, view.m[0][1], view.m[1][1], view.m[2][1]));
    
    // and update our eye position accordingly
    mat4ApplyToVec3(&camera_eye, &avector, &M);
    vec3Add(&camera_eye, &camera_lookat);
  };

  if ((moveVert <= -0.1) || (moveVert >= 0.1)) {
    // get our (reverse) looking direction vector
    vec3Copy(&avector, &camera_eye);
    vec3Sub(&avector, &camera_lookat);
    
    // rotate our looking direction vector around our right vector
    mat4Identity(&M);
    mat4Rotate(&M, moveVert, vec3Set(&bvector, view.m[0][0], view.m[1][0], view.m[2][0]));
    
    // and update our eye position accordingly
    mat4ApplyToVec3(&camera_eye, &avector, &M);
    vec3Add(&camera_eye, &camera_lookat);
  };

  if ((moveForward <= -0.1) || (moveForward >= 0.1)) {
    vec3Set(&avector, view.m[0][2], view.m[1][2], view.m[2][2]); // our forward vector
    vec3Scale(&avector, moveForward);

    vec3Add(&camera_eye, &avector);
    vec3Add(&camera_lookat, &avector);
  };

  if ((moveSideways <= -0.1) || (moveSideways >= 0.1)) {
    vec3Set(&avector, view.m[0][0], view.m[1][0], view.m[2][0]); // our right vector
    vec3Scale(&avector, moveSideways);

    vec3Add(&camera_eye, &avector);
    vec3Add(&camera_lookat, &avector);
  };

  // update our view matrix
  mat4Identity(&view);
  mat4LookAt(&view, &camera_eye, &camera_lookat, vec3Set(&upvector, 0.0, 1.0, 0.0));
    
  // update our frame counter
  frames += 1.0f;
  delta = pSecondsPassed - lastsecs;
  fps = (frames - lastframes) / delta;

  if (frames - lastframes > 100.0f) {
    // reset every 100 frames
    lastsecs = pSecondsPassed;
    lastframes = frames;
  };
};

// engineRender is called to render our stuff
// pWidth and pHeight define the size of our drawing buffer
// pMode is 0 => mono, 1 => left eye, 2 => right eye
void engineRender(int pWidth, int pHeight, float pRatio, int pMode) {
  mat4            tmpmatrix;
  shaderMatrices  matrices;
  vec3            tmpvector;
  float           left, top;
  int             i;

  // calculate our sun position, we want to do this only once
  mat4ApplyToVec3(&sun.adjPosition, &sun.position, &view);
        
  // enable and configure our backface culling
  glEnable(GL_CULL_FACE);   // enable culling
  glFrontFace(GL_CW);       // clockwise
  glCullFace(GL_BACK);      // backface culling

  // enable our depth test
  glEnable(GL_DEPTH_TEST);
  // disable alpha blending  
  glDisable(GL_BLEND);

  if (wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);    
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  };

  // init our projection matrix, we use a 3D projection matrix now
  mat4Identity(&tmpmatrix);
  // distance between eyes is on average 6.5 cm, this should be setable
  mat4Stereo(&tmpmatrix, 45.0, pRatio, 1.0, 100000.0, 6.5, 200.0, pMode);
  shdMatSetProjection(&matrices, &tmpmatrix); // call our set function to reset our flags
  
  // copy our view matrix into our state
  shdMatSetView(&matrices, &view);
  
  // and render our scene
  if (scene != NULL) {
    meshNodeRender(scene, &matrices, (material *) materials->first->data, &sun);    

    // This may have been turned on in our node renderer
    glDisable(GL_BLEND);
  };
  
  // unset stuff
  glBindVertexArray(0);
  glUseProgram(0);
  
  ////////////////////////////////////
  // UI
    
  // change our state a little
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);  
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
 
  // now render our FPS
	if ((fs != NULL) && (font != FONS_INVALID)) {
    const joystickInfo * joystick = getJoystickInfo(0);
    float virtualScreenHeight = 250.0;
    char info[256];
    
    // We want a orthographic projection for our frame counter
    mat4Identity(&matrices.projection);
    mat4Ortho(&matrices.projection, -pRatio * virtualScreenHeight, pRatio * virtualScreenHeight, virtualScreenHeight, -virtualScreenHeight, 1.0f, -1.0f);
    gl3fonsProjection(fs, (GLfloat *)matrices.projection.m);

    // what text shall we draw?
    sprintf(info,"FPS: %0.1f, use wasd to rotate the camera, zc to move forwards/backwards. f to toggle wireframe", fps);
    fonsDrawText(fs, -pRatio * 250.0f, 230.0f, info, NULL);
    
    // lets display some info about our joystick:
    if (joystick != NULL) {
      if (joystick->enabled) {
        sprintf(info, "Joystick %s is active", joystick->name);
        fonsDrawText(fs, -pRatio * 250.0f, -250.0f, info, NULL);

        sprintf(info, "Axes: %f %f %f %f %f %f %f %f", joystick->axes[0], joystick->axes[1], joystick->axes[2], joystick->axes[3], joystick->axes[4], joystick->axes[5], joystick->axes[6], joystick->axes[7]);
        fonsDrawText(fs, -pRatio * 250.0f, -230.0f, info, NULL);

        sprintf(info, "Buttons: %i %i %i %i %i %i %i %i", joystick->buttons[0], joystick->buttons[1], joystick->buttons[2], joystick->buttons[3], joystick->buttons[4], joystick->buttons[5], joystick->buttons[6], joystick->buttons[7]);
        fonsDrawText(fs, -pRatio * 250.0f, -210.0f, info, NULL);        
      } else {
        sprintf(info, "Joystick %s is inactive", joystick->name);
        fonsDrawText(fs, -pRatio * 250.0f, -250.0f, info, NULL);
      };
    };
  };
};

void engineKeyPressed(int pKey) {
//  pushKey(pKey);
  if (pKey == GLFW_KEY_F) {
    // toggle wireframe
    wireframe = !wireframe;
  };
};
