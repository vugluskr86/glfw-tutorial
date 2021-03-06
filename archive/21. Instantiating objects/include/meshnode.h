/********************************************************
 * meshnode.h - node library by Bastiaan Olij 2016
 * 
 * Public domain, use as you say fit, disect, change,
 * or otherwise, all at your own risk
 *
 * This library is given as a single file implementation.
 * Include this in any file that requires it but in one
 * file, and one file only, proceed it with:
 * #define MESH_IMPLEMENTATION
 *
 * Note that OpenGL headers need to be included before 
 * this file is included as it uses several of its 
 * functions. 
 *
 * Revision history:
 * 0.1  08-02-2016  First version with basic functions
 * 0.2  13-02-2016  Added copy function
 *
 ********************************************************/

#ifndef meshnodeh
#define meshnodeh

#include "errorlog.h"
#include "linkedlist.h"
#include "dynamicarray.h"
#include "varchar.h"
#include "math3d.h"
#include "material.h"
#include "mesh3d.h"

// structure for managing instances of mesh data
typedef struct meshNode {
  unsigned int  retainCount;          /* retain count for this object */
  bool          visible;              /* if true we render the mesh(es) contained within this node */
  char          name[250];            /* name for this node */
  
  // our positioning matrix
  mat4          position;             /* position relative to our parent instance */
  
  // mesh to render
  mesh3d *      mesh;                 /* mesh to render, NULL is just a positioning node */
  
  // children
  llist *       children;             /* child nodes */
} meshNode;

#ifdef __cplusplus
extern "C" {
#endif

meshNode * newMeshNode(const char * pName);
meshNode * newCopyMeshNode(const char *pName, meshNode * pCopy, bool pDeepCopy);
llist * newMeshNodeList(void);
void meshNodeRetain(meshNode * pNode);
void meshNodeRelease(meshNode * pNode);
void meshNodeSetMesh(meshNode * pNode, mesh3d * pMesh);
void meshNodeAddChild(meshNode * pNode, meshNode * pChild);
void meshNodeAddChildren(meshNode *pNode, llist * pMeshList);
void meshNodeRender(meshNode * pNode, shaderMatrices pMatrices, material * pDefaultMaterial, lightSource * pSun);

#ifdef __cplusplus
};
#endif

#ifdef MESH_IMPLEMENTATION

// create a new mesh node
meshNode * newMeshNode(const char * pName) {
  meshNode * newNode = (meshNode *) malloc(sizeof(meshNode));
  if (newNode != NULL) {
    newNode->retainCount = 1;
    newNode->visible = true;
    strcpy(newNode->name, pName);
    mat4Identity(&newNode->position);
    newNode->mesh = NULL;
    newNode->children = newMeshNodeList();
  };
  
  return newNode;
};

// copy a mesh node, if pDeepCopy is false we reference the same child nodes, if pDeepCopy is true we make copies of all the child nodes (not the meshes)
meshNode * newCopyMeshNode(const char *pName, meshNode * pCopy, bool pDeepCopy) {
  if (pCopy == NULL) {
    errorlog(-1, "Attempted to copy NULL node");
    return NULL;
  };

  meshNode * newNode = (meshNode *) malloc(sizeof(meshNode));
  if (newNode != NULL) {
    llistNode * lnode;
    newNode->retainCount = 1;
    newNode->visible = pCopy->visible;
    strcpy(newNode->name, pName);
    mat4Copy(&newNode->position, &pCopy->position);
    newNode->mesh = NULL; /* start NULL! */
    meshNodeSetMesh(newNode, pCopy->mesh); /* now assign our mesh, note that we're thus retaining the same mesh as the node we're copying*/
    newNode->children = newMeshNodeList();

    // now copy our children
    lnode = pCopy->children->first;
    while (lnode != NULL) {
      meshNode * child = (meshNode *) lnode->data;

      // make a copy of our child
      if (pDeepCopy) {
        child = newCopyMeshNode(child->name, child, true);
      };

      // and add our child to our new node
      if (child != NULL) {
        meshNodeAddChild(newNode, child);
      };

      lnode = lnode->next;
    };
  };
  
  return newNode;
};

// create a linked list containing mesh nodes
llist * newMeshNodeList(void) {
  llist * nodeList = newLlist((dataRetainFunc) meshNodeRetain, (dataFreeFunc) meshNodeRelease);
  return nodeList;  
};

// retain a mesh node
void meshNodeRetain(meshNode * pNode) {
  if (pNode == NULL) {
    errorlog(-1, "Attempted to retain NULL node");
  } else {
    pNode->retainCount++;    
  };  
};

// release a mesh node and free it if it reaches a retain count of 0
void meshNodeRelease(meshNode * pNode) {
  if (pNode == NULL) {
    errorlog(-1, "Attempted to release NULL node");
    return;
  } else if (pNode->retainCount > 1) {
    pNode->retainCount--;
    
    return;
  } else {
    // free our mesh if its set
    meshNodeSetMesh(pNode, NULL);
    
    // free our children
    if (pNode->children != NULL) {
      llistFree(pNode->children);
      pNode->children = NULL;      
    };
    
    free(pNode);
  };
};

// set (or unset) our related mesh, the mesh will be release/retained as needed
void meshNodeSetMesh(meshNode * pNode, mesh3d * pMesh) {
  if (pNode == NULL) {
    errorlog(-1, "Attempted to set a mesh to a NULL node");
    return;
  } else if (pNode->mesh == pMesh) {
    return;
  } else {
    if (pNode->mesh != NULL) {
      meshRelease(pNode->mesh);
    };
    pNode->mesh = pMesh;
    if (pNode->mesh != NULL) {
      meshRetain(pNode->mesh);
    };    
  };  
};

// add a child node to our node
void meshNodeAddChild(meshNode * pNode, meshNode * pChild) {
  if (pNode == NULL) {
    errorlog(-1, "Attempted to add a child node to a NULL node");
    return;
  } else if (pChild == NULL) {
    errorlog(-1, "Attempted to add a NULL node");
    return;
  } else {
    llistAddTo(pNode->children, pChild);
  };
};

// add children to our node for all the meshes contained in the mesh list
void meshNodeAddChildren(meshNode *pNode, llist * pMeshList) {
  if (pNode == NULL) {
    errorlog(-1, "Attempted to add meshes to NULL node");
    return;
  } else if (pMeshList == NULL) {
    errorlog(-1, "No meshes to add to node");
    return;
  } else {
    llistNode * node = pMeshList->first;
    while (node != NULL) {
      mesh3d * mesh = (mesh3d *) node->data;
      meshNode * newNode = newMeshNode(mesh->name);
      
      mat4Copy(&newNode->position, &mesh->defModel);
      meshNodeSetMesh(newNode, mesh);
      
      meshNodeAddChild(pNode, newNode);
      meshNodeRelease(newNode);   // now retained by our children linked list
      
      node = node->next;
    };
  };
};

// structure for storing info about transparent meshes that we render later on
typedef struct renderMesh {
  mesh3d *  mesh;
  mat4      model;
  GLfloat   z;
} renderMesh;

// build our no-alpha and alpha render lists based on the contents of our node
void meshNodeBuildRenderList(meshNode * pNode, mat4 * pModel, dynarray * pNoAlpha, dynarray * pAlpha) {
  mat4 model;
  
  // is there anything to do?
  if (pNode == NULL) {
    return;
  } else if (pNode->visible == false) {
    return;
  };
  
  // make our model matrix
  mat4Copy(&model, pModel);
  mat4Multiply(&model, &pNode->position);
  
  if (pNode->mesh != NULL) {
    // add our mesh
    renderMesh render;
    render.mesh = pNode->mesh;
    mat4Copy(&render.model, &model);
    render.z = 0.0; // not yet used, need to apply view matrix to calculate
    
    if (pNode->mesh->material == NULL) {
      dynArrayPush(pNoAlpha, &render); // this copies our structure
    } else if (pNode->mesh->material->alpha != 1.0) {
      dynArrayPush(pAlpha, &render); // this copies our structure      
    } else {
      dynArrayPush(pNoAlpha, &render); // this copies our structure      
    }
  };
  
  if (pNode->children != NULL) {
    llistNode * node = pNode->children->first;
    
    while (node != NULL) {
      meshNodeBuildRenderList((meshNode *) node->data, &model, pNoAlpha, pAlpha);
      node = node->next;
    };
  };
};

// render the contents of our node to the current output
void meshNodeRender(meshNode * pNode, shaderMatrices pMatrices, material * pDefaultMaterial, lightSource * pSun) {
  dynarray *      meshesWithoutAlpha  = newDynArray(sizeof(renderMesh));
  dynarray *      meshesWithAlpha     = newDynArray(sizeof(renderMesh));
  mat4            model;
  int             i;

  // prepare our array with things to render....
  mat4Identity(&model);
  meshNodeBuildRenderList(pNode, &model, meshesWithoutAlpha, meshesWithAlpha);
  
  // now render no-alpha
  glDisable(GL_BLEND);

  for (i = 0; i < meshesWithoutAlpha->numEntries; i++) {
    renderMesh * render = dynArrayDataAtIndex(meshesWithoutAlpha, i);
  
    mat4Copy(&pMatrices.model, &render->model);
    if (render->mesh->material == NULL) {
      matSelectProgram(pDefaultMaterial, &pMatrices, pSun);
    } else {
      matSelectProgram(render->mesh->material, &pMatrices, pSun);
    };

    meshRender(render->mesh);
  };    
  
  
  // and render alpha
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  
  for (i = 0; i < meshesWithAlpha->numEntries; i++) {
    renderMesh * render = dynArrayDataAtIndex(meshesWithAlpha, i);
  
    mat4Copy(&pMatrices.model, &render->model);
    matSelectProgram(render->mesh->material, &pMatrices, pSun);

    meshRender(render->mesh);
  };
  
  dynArrayFree(meshesWithAlpha);
  dynArrayFree(meshesWithoutAlpha);
};

#endif /* MESH_IMPLEMENTATION */

#endif /* !meshnodeh */