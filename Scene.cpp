//--------------------------------------------------------------------------------------
// Scene geometry and layout preparation
// Scene rendering & update
//--------------------------------------------------------------------------------------

#include "Scene.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "State.h"
#include "Shader.h"
#include "Input.h"
#include "Common.h"

#include "CVector2.h" 
#include "CVector3.h" 
#include "CMatrix4x4.h"
#include "MathHelpers.h"     // Helper functions for maths
#include "GraphicsHelpers.h" // Helper functions to unclutter the code here

#include "ColourRGBA.h" 

#include <sstream>
#include <memory>


//--------------------------------------------------------------------------------------
// Scene Data
//--------------------------------------------------------------------------------------
// Addition of Mesh, Model and Camera classes have greatly simplified this section
// Geometry data has gone to Mesh class. Positions, rotations, matrices have gone to Model and Camera classes

// Constants controlling speed of movement/rotation (measured in units per second because we're using frame time)
const float ROTATION_SPEED = 2.0f;  // 2 radians per second for rotation
const float MOVEMENT_SPEED = 50.0f; // 50 units per second for movement (what a unit of length is depends on 3D model - i.e. an artist decision usually)


// Meshes, models and cameras, same meaning as TL-Engine. Meshes prepared in InitGeometry function, Models & camera in InitScene
Mesh* gCubeMesh;
Mesh* gDecalMesh;
Mesh* gCrateMesh;
Mesh* gSphereMesh;
Mesh* gGroundMesh;
Mesh* gLightMesh;
Mesh* gPortalMesh;
Mesh* gPortal2Mesh;

Model* gCube;
Model* gDecal;
Model* gCrate;
Model* gSphere;
Model* gGround;
Model* gLight1;
Model* gLight2;
Model* gPortal;
Model* gPortal2;

// Two cameras now. The main camera, and the view through the portal
Camera* gCamera;
Camera* gPortalCamera;
Camera* gPortal2Camera;


// Additional light information
CVector3 gLight1Colour = { 0.8f, 0.8f, 1.0f };
float    gLight1Strength = 10; // Allows the light to be stronger or weaker - also controls the light model scale

CVector3 gLight2Colour = { 1.0f, 0.8f, 0.2f };
float    gLight2Strength = 40;

CVector3 gAmbientColour = { 0.2f, 0.2f, 0.3f }; // Background level of light (slightly bluish to match the far background, which is dark blue)
float    gSpecularPower = 256; // Specular power controls shininess - same for all models in this app

ColourRGBA gBackgroundColor = { 0.2f, 0.2f, 0.3f , 1.0f };


// Variables controlling light1's orbiting of the cube
const float gLightOrbit = 20.0f;
const float gLightOrbitSpeed = 0.7f;


// Lock FPS to monitor refresh rate, which will typically set it to 60fps. Press 'p' to toggle to full fps
bool lockFPS = true;


//--------------------------------------------------------------------------------------
//**** Portal Texture  ****//
//--------------------------------------------------------------------------------------
// This texture will have the scene renderered on it. Then the texture is applied to a model

// Dimensions of portal texture - controls quality of rendered scene in portal
int gPortalWidth  = 512;
int gPortalHeight = 512;

// The portal texture - each frame it is rendered to, then it is used as a texture for model
ID3D11Texture2D*          gPortalTexture      = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11Texture2D*          gPortal2Texture      = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11RenderTargetView*   gPortalRenderTarget = nullptr; // This object is used when we want to render to the texture above
ID3D11RenderTargetView*   gPortal2RenderTarget = nullptr; // This object is used when we want to render to the texture above
ID3D11ShaderResourceView* gPortalTextureSRV   = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)
ID3D11ShaderResourceView* gPortal2TextureSRV   = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

// Also need a depth/stencil buffer for the portal - it's just another kind of texture
// NOTE: ***Can share this depth buffer between multiple portals of the same size***
ID3D11Texture2D*        gPortalDepthStencil     = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11DepthStencilView* gPortalDepthStencilView = nullptr; // This object is used when we want to use the texture above as the depth buffer

//*********************//


//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
// Variables sent over to the GPU each frame
// The structures are now in Common.h
// IMPORTANT: Any new data you add in C++ code (CPU-side) is not automatically available to the GPU
//            Anything the shaders need (per-frame or per-model) needs to be sent via a constant buffer

PerFrameConstants gPerFrameConstants;      // The constants that need to be sent to the GPU each frame (see common.h for structure)
ID3D11Buffer*     gPerFrameConstantBuffer; // The GPU buffer that will recieve the constants above

PerModelConstants gPerModelConstants;      // As above, but constant that change per-model (e.g. world matrix)
ID3D11Buffer*     gPerModelConstantBuffer; // --"--



//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------

// DirectX objects controlling textures used in this lab
ID3D11Resource*           gCubeDiffuseSpecularMap    = nullptr; // This object represents the memory used by the texture on the GPU
ID3D11ShaderResourceView* gCubeDiffuseSpecularMapSRV = nullptr; // This object is used to give shaders access to the texture above (SRV = shader resource view)

ID3D11Resource*           gDecalDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gDecalDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gCrateDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gCrateDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gSphereDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gSphereDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gGroundDiffuseSpecularMap    = nullptr;
ID3D11ShaderResourceView* gGroundDiffuseSpecularMapSRV = nullptr;

ID3D11Resource*           gLightDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gLightDiffuseMapSRV = nullptr;

ID3D11Resource*           gTvDiffuseMap    = nullptr;
ID3D11ShaderResourceView* gTvDiffuseMapSRV = nullptr;


//--------------------------------------------------------------------------------------
// Initialise scene geometry, constant buffers and states
//--------------------------------------------------------------------------------------

// Prepare the geometry required for the scene
// Returns true on success
bool InitGeometry()
{
    // Load mesh geometry data, just like TL-Engine this doesn't create anything in the scene. Create a Model for that.
    // IMPORTANT NOTE: Will only keep the first object from the mesh - multipart objects will have parts missing - see later lab for more robust loader
    try 
    {
        gCubeMesh   = new Mesh("Cube.x");
        gDecalMesh  = new Mesh("Decal.x");
        gCrateMesh  = new Mesh("CargoContainer.x");
        gSphereMesh = new Mesh("Sphere.x");
        gGroundMesh = new Mesh("Hills.x");
        gLightMesh  = new Mesh("Light.x");
        gPortalMesh = new Mesh("Portal.x");
        gPortal2Mesh = new Mesh("Portal.x");
    }
    catch (std::runtime_error e)  // Constructors cannot return error messages so use exceptions to catch mesh errors (fairly standard approach this)
    {
        gLastError = e.what(); // This picks up the error message put in the exception (see Mesh.cpp)
        return false;
    }


    // Load the shaders required for the geometry we will use (see Shader.cpp / .h)
    if (!LoadShaders())
    {
        gLastError = "Error loading shaders";
        return false;
    }


    // Create GPU-side constant buffers to receive the gPerFrameConstants and gPerModelConstants structures above
    // These allow us to pass data from CPU to shaders such as lighting information or matrices
    // See the comments above where these variable are declared and also the UpdateScene function
    gPerFrameConstantBuffer = CreateConstantBuffer(sizeof(gPerFrameConstants));
    gPerModelConstantBuffer = CreateConstantBuffer(sizeof(gPerModelConstants));
    if (gPerFrameConstantBuffer == nullptr || gPerModelConstantBuffer == nullptr)
    {
        gLastError = "Error creating constant buffers";
        return false;
    }


    //// Load / prepare textures on the GPU ////

    // Load textures and create DirectX objects for them
    // The LoadTexture function requires you to pass a ID3D11Resource* (e.g. &gCubeDiffuseMap), which manages the GPU memory for the
    // texture and also a ID3D11ShaderResourceView* (e.g. &gCubeDiffuseMapSRV), which allows us to use the texture in shaders
    // The function will fill in these pointers with usable data. The variables used here are globals found near the top of the file.
    if (!LoadTexture("StoneDiffuseSpecular.dds", &gCubeDiffuseSpecularMap,   &gCubeDiffuseSpecularMapSRV   ) ||
        !LoadTexture("Moogle.png",               &gDecalDiffuseSpecularMap,  &gDecalDiffuseSpecularMapSRV  ) ||
        !LoadTexture("CargoA.dds",               &gCrateDiffuseSpecularMap,  &gCrateDiffuseSpecularMapSRV  ) ||
        !LoadTexture("Brick1.jpg",               &gSphereDiffuseSpecularMap, &gSphereDiffuseSpecularMapSRV ) ||
        !LoadTexture("GrassDiffuseSpecular.dds", &gGroundDiffuseSpecularMap, &gGroundDiffuseSpecularMapSRV ) ||
        !LoadTexture("Flare.jpg",                &gLightDiffuseMap,          &gLightDiffuseMapSRV          ) ||
        !LoadTexture("tv.dds",                   &gTvDiffuseMap,             &gTvDiffuseMapSRV             ))
    {
        gLastError = "Error loading textures";
        return false;
    }




    //**** Create Portal Texture ****//

	// Using a helper function to load textures from files above. Here we create the portal texture manually
	// as we are creating a special kind of texture (one that we can render to). Many settings to prepare:
	D3D11_TEXTURE2D_DESC portalDesc = {};
	portalDesc.Width  = gPortalWidth;  // Size of the portal texture determines its quality
	portalDesc.Height = gPortalHeight;
	portalDesc.MipLevels = 1; // No mip-maps when rendering to textures (or we would have to render every level)
	portalDesc.ArraySize = 1;
	portalDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBA texture (8-bits each)
	portalDesc.SampleDesc.Count = 1;
	portalDesc.SampleDesc.Quality = 0;
	portalDesc.Usage = D3D11_USAGE_DEFAULT;
	portalDesc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE; // IMPORTANT: Indicate we will use texture as render target, and pass it to shaders
	portalDesc.CPUAccessFlags = 0;
	portalDesc.MiscFlags = 0;
	if (FAILED( gD3DDevice->CreateTexture2D(&portalDesc, NULL, &gPortalTexture) ))
	{
		gLastError = "Error creating portal texture";
		return false;
	}
    if (FAILED(gD3DDevice->CreateTexture2D(&portalDesc, NULL, &gPortal2Texture)))
    {
        gLastError = "Error creating portal texture";
        return false;
    }

	// We created the portal texture above, now we get a "view" of it as a render target, i.e. get a special pointer to the texture that
	// we use when rendering to it (see RenderScene function below)
	if (FAILED( gD3DDevice->CreateRenderTargetView(gPortalTexture, NULL, &gPortalRenderTarget) ))
	{
		gLastError = "Error creating portal render target view";
		return false;
	}
    if (FAILED(gD3DDevice->CreateRenderTargetView(gPortal2Texture, NULL, &gPortal2RenderTarget)))
    {
        gLastError = "Error creating portal render target view";
        return false;
    }

	// We also need to send this texture (resource) to the shaders. To do that we must create a shader-resource "view"
	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc = {};
	srDesc.Format = portalDesc.Format;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;
	if (FAILED( gD3DDevice->CreateShaderResourceView(gPortalTexture, &srDesc, &gPortalTextureSRV) ))
	{
		gLastError = "Error creating portal shader resource view";
		return false;
	}

    if (FAILED(gD3DDevice->CreateShaderResourceView(gPortal2Texture, &srDesc, &gPortal2TextureSRV)))
    {
        gLastError = "Error creating portal shader resource view";
        return false;
    }


	//**** Create Portal Depth Buffer ****//

	// We also need a depth buffer to go with our portal
	//**** This depth buffer can be shared with any other portals of the same size
    portalDesc = {};
	portalDesc.Width  = gPortalWidth;
	portalDesc.Height = gPortalHeight;
	portalDesc.MipLevels = 1;
	portalDesc.ArraySize = 1;
	portalDesc.Format = DXGI_FORMAT_D32_FLOAT; // Depth buffers contain a single float per pixel
	portalDesc.SampleDesc.Count = 1;
	portalDesc.SampleDesc.Quality = 0;
	portalDesc.Usage = D3D11_USAGE_DEFAULT;
	portalDesc.BindFlags = D3D10_BIND_DEPTH_STENCIL;
	portalDesc.CPUAccessFlags = 0;
	portalDesc.MiscFlags = 0;
	if (FAILED(gD3DDevice->CreateTexture2D(&portalDesc, NULL, &gPortalDepthStencil) ))
	{
		gLastError = "Error creating portal depth stencil texture";
		return false;
	}

	// Create the depth stencil view, i.e. indicate that the texture just created is to be used as a depth buffer
	D3D11_DEPTH_STENCIL_VIEW_DESC portalDescDSV = {};
	portalDescDSV.Format = portalDesc.Format;
	portalDescDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	portalDescDSV.Texture2D.MipSlice = 0;
    portalDescDSV.Flags = 0;
	if (FAILED(gD3DDevice->CreateDepthStencilView(gPortalDepthStencil, &portalDescDSV, &gPortalDepthStencilView) ))
	{
		gLastError = "Error creating portal depth stencil view";
		return false;
	}

    
    //*****************************//
    


  	// Create all filtering modes, blending modes etc. used by the app (see State.cpp/.h)
	if (!CreateStates())
	{
		gLastError = "Error creating states";
		return false;
	}

	return true;
}


// Prepare the scene
// Returns true on success
bool InitScene()
{
    //// Set up scene ////

    gCube   = new Model(gCubeMesh);
    gDecal  = new Model(gDecalMesh);
    gCrate  = new Model(gCrateMesh);
    gSphere = new Model(gSphereMesh);
    gGround = new Model(gGroundMesh);
    gLight1 = new Model(gLightMesh);
    gLight2 = new Model(gLightMesh);
    gPortal = new Model(gCubeMesh);
    gPortal2 = new Model(gPortalMesh);

	// Initial positions
	gCube->  SetPosition({  0, 15,  0 });
	gDecal-> SetPosition({  0, 15,  -0.2 });
	gSphere->SetPosition({ 30, 10,  0 });
	gCrate-> SetPosition({-10,  0, 90 });
	gCrate-> SetScale( 6.0f );
	gCrate-> SetRotation({ 0.0f, ToRadians(40.0f), 0.0f });
	gPortal->SetPosition({ 40, 20, 40 });
	gPortal->SetRotation({ 0.0f, ToRadians(-130.0f), 0.0f });
    gPortal2->SetPosition({ 50, 20, 40 });
    gPortal2->SetRotation({ 0.0f, ToRadians(-130.0f), 0.0f });

    gLight1->SetPosition({ 30, 10, 0 });
    gLight1->SetScale(pow(gLight1Strength, 0.7f)); // Convert light strength into a nice value for the scale of the light - equation is ad-hoc.
    gLight2->SetPosition({ -20, 30, 40 });
    gLight2->SetScale(pow(gLight2Strength, 0.7f));


    //// Set up cameras ////

    gCamera = new Camera();
    gCamera->SetPosition({ 40, 30, -90 });
    gCamera->SetRotation({ ToRadians(8.0f), ToRadians(-18.0f), 0.0f });
	gCamera->SetNearClip( 1.0f );
	gCamera->SetFarClip( 200.0f );


  	//**** Portal camera is the view shown in the portal object's texture ****//
	gPortalCamera = new Camera();
	gPortalCamera->SetPosition({ 45, 45, 85 });
	gPortalCamera->SetRotation({ ToRadians(20.0f), ToRadians(215.0f), 0 });

    gPortal2Camera = new Camera();
    gPortal2Camera->SetPosition({ 45, 45, 85 });
    gPortal2Camera->SetRotation({ ToRadians(20.0f), ToRadians(215.0f), 0 });

    return true;
}


// Release the geometry and scene resources created above
void ReleaseResources()
{
    ReleaseStates();

    if (gPortalDepthStencilView)  gPortalDepthStencilView->Release();
    if (gPortalDepthStencil)      gPortalDepthStencil->Release();
    if (gPortalTextureSRV)        gPortalTextureSRV->Release();
    if (gPortal2TextureSRV)        gPortal2TextureSRV->Release();
    if (gPortalRenderTarget)      gPortalRenderTarget->Release();
    if (gPortal2RenderTarget)      gPortal2RenderTarget->Release();
    if (gPortalTexture)           gPortalTexture->Release();
    if (gPortal2Texture)           gPortal2Texture->Release();

    if (gLightDiffuseMapSRV)           gLightDiffuseMapSRV->Release();
    if (gLightDiffuseMap)              gLightDiffuseMap->Release();
    if (gGroundDiffuseSpecularMapSRV)  gGroundDiffuseSpecularMapSRV->Release();
    if (gGroundDiffuseSpecularMap)     gGroundDiffuseSpecularMap->Release();
    if (gSphereDiffuseSpecularMapSRV)  gSphereDiffuseSpecularMapSRV->Release();
    if (gSphereDiffuseSpecularMap)     gSphereDiffuseSpecularMap->Release();
    if (gCrateDiffuseSpecularMapSRV)   gCrateDiffuseSpecularMapSRV->Release();
    if (gCrateDiffuseSpecularMap)      gCrateDiffuseSpecularMap->Release();
    if (gDecalDiffuseSpecularMapSRV)   gDecalDiffuseSpecularMapSRV->Release();
    if (gDecalDiffuseSpecularMap)      gDecalDiffuseSpecularMap->Release();
    if (gCubeDiffuseSpecularMapSRV)    gCubeDiffuseSpecularMapSRV->Release();
    if (gCubeDiffuseSpecularMap)       gCubeDiffuseSpecularMap->Release();

    if (gPerModelConstantBuffer)  gPerModelConstantBuffer->Release();
    if (gPerFrameConstantBuffer)  gPerFrameConstantBuffer->Release();

    ReleaseShaders();

    // See note in InitGeometry about why we're not using unique_ptr and having to manually delete
    delete gCamera;        gCamera       = nullptr;
    delete gPortalCamera;  gPortalCamera = nullptr;
    delete gPortal2Camera;  gPortal2Camera = nullptr;

    delete gPortal;  gPortal = nullptr;
    delete gLight1;  gLight1 = nullptr;
    delete gLight2;  gLight2 = nullptr;
    delete gGround;  gGround = nullptr;
    delete gSphere;  gSphere = nullptr;
    delete gCrate;   gCrate  = nullptr;
    delete gDecal;   gDecal  = nullptr;
    delete gCube;    gCube   = nullptr;

    delete gPortalMesh;  gPortalMesh = nullptr;
    delete gLightMesh;   gLightMesh  = nullptr;
    delete gGroundMesh;  gGroundMesh = nullptr;
    delete gSphereMesh;  gSphereMesh = nullptr;
    delete gCrateMesh;   gCrateMesh  = nullptr;
    delete gDecalMesh;   gDecalMesh  = nullptr;
    delete gCubeMesh;    gCubeMesh   = nullptr;
}



//--------------------------------------------------------------------------------------
// Scene Rendering
//--------------------------------------------------------------------------------------


// Render everything in the scene from the given camera
// This code is common between rendering the main scene and rendering the scene in the portal
// See RenderScene function below
void RenderSceneFromCamera(Camera* camera)
{
    // Set camera matrices in the constant buffer and send over to GPU
    gPerFrameConstants.viewMatrix           = camera->ViewMatrix();
    gPerFrameConstants.projectionMatrix     = camera->ProjectionMatrix();
    gPerFrameConstants.viewProjectionMatrix = camera->ViewProjectionMatrix();
    UpdateConstantBuffer(gPerFrameConstantBuffer, gPerFrameConstants);

    // Indicate that the constant buffer we just updated is for use in the vertex shader (VS) and pixel shader (PS)
    gD3DContext->VSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer); // First parameter must match constant buffer number in the shader 
    gD3DContext->PSSetConstantBuffers(0, 1, &gPerFrameConstantBuffer);


    //// Render lit models - ground first ////

    // Select which shaders to use next
    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader,  nullptr, 0);
    
    // States - no blending, normal depth buffer and culling
    gD3DContext->OMSetBlendState(gNoBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gUseDepthBufferState, 0);
    gD3DContext->RSSetState(gCullBackState);

    // Select the approriate textures and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gGroundDiffuseSpecularMapSRV); // First parameter must match texture slot number in the shader
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    // Render model - it will update the model's world matrix and send it to the GPU in a constant buffer, then it will call
    // the Mesh render function, which will set up vertex & index buffer before finally calling Draw on the GPU
    gGround->Render();


    //// Render other lit models next ////
    // No change to shaders and states - only change textures for each one (texture sampler also stays the same)

    gD3DContext->PSSetShaderResources(0, 1, &gCubeDiffuseSpecularMapSRV); 
    gCube->Render();

    gD3DContext->PSSetShaderResources(0, 1, &gCrateDiffuseSpecularMapSRV);
    gCrate->Render();

    gD3DContext->PSSetShaderResources(0, 1, &gSphereDiffuseSpecularMapSRV);
    gSphere->Render();



	//**** MISSING - Render the portal object here ****//

    gD3DContext->PSSetShaderResources(0, 1, &gPortalTextureSRV);
    gPortal2->Render();

    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gTvPixelShader, nullptr, 0);

    gD3DContext->PSSetShaderResources(0, 1, &gPortalTextureSRV);
    gD3DContext->PSSetShaderResources(1, 1, &gTvDiffuseMapSRV);

    gPortal->Render();





    //// Render decal ////
    // No change to shaders, but states are different (additive blending)

    gD3DContext->VSSetShader(gPixelLightingVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gPixelLightingPixelShader, nullptr, 0);

    // States - additive blending, read-only depth buffer and no culling (standard set-up for blending
    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    // Select the texture and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gDecalDiffuseSpecularMapSRV);

    // Render model, sets world matrix, vertex and index buffer and calls Draw on the GPU
    gDecal->Render();


    //// Render lights ////
    // Rendered with different shaders, textures, states from other models

    // Select which shaders to use next
    gD3DContext->VSSetShader(gLightModelVertexShader, nullptr, 0);
    gD3DContext->PSSetShader(gLightModelPixelShader,  nullptr, 0);

    // Select the texture and sampler to use in the pixel shader
    gD3DContext->PSSetShaderResources(0, 1, &gLightDiffuseMapSRV); // First parameter must match texture slot number in the shaer
    gD3DContext->PSSetSamplers(0, 1, &gAnisotropic4xSampler);

    // States - additive blending, read-only depth buffer and no culling (standard set-up for blending
    gD3DContext->OMSetBlendState(gAdditiveBlendingState, nullptr, 0xffffff);
    gD3DContext->OMSetDepthStencilState(gDepthReadOnlyState, 0);
    gD3DContext->RSSetState(gCullNoneState);

    // Render model, sets world matrix, vertex and index buffer and calls Draw on the GPU
    gPerModelConstants.objectColour = gLight1Colour; // Set any per-model constants apart from the world matrix just before calling render (light colour here)
    gLight1->Render();

    // The shaders, texture and states are the same, so no need to set them again to draw the second light
    gPerModelConstants.objectColour = gLight2Colour;
    gLight2->Render();
}




// Rendering the scene now renders everything twice. First it renders the scene for the portal into a texture.
// Then it renders the main scene using the portal texture on a model.
void RenderScene()
{
    //// Common settings for both main scene and portal scene ////

    // Set up the light information in the constant buffer - this is the same for portal and main render
    // Don't send to the GPU yet, the function RenderSceneFromCamera will do that
    gPerFrameConstants.light1Colour   = gLight1Colour * gLight1Strength;
    gPerFrameConstants.light1Position = gLight1->Position();
    gPerFrameConstants.light2Colour   = gLight2Colour * gLight2Strength;
    gPerFrameConstants.light2Position = gLight2->Position();
    gPerFrameConstants.ambientColour  = gAmbientColour;
    gPerFrameConstants.specularPower  = gSpecularPower;
    gPerFrameConstants.cameraPosition = gCamera->Position();


    //-------------------------------------------------------------------------


    //// Portal scene rendering ////

    // Set the portal texture and portal depth buffer as the targets for rendering
    // The portal texture will later be used on models in the main scene
    gD3DContext->OMSetRenderTargets(1, &gPortalRenderTarget, gPortalDepthStencilView);

    // Clear the portal texture to a fixed colour and the portal depth buffer to the far distance
    gD3DContext->ClearRenderTargetView(gPortalRenderTarget, &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gPortalDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Setup the viewport for the portal texture size
    D3D11_VIEWPORT vp;
    vp.Width  = static_cast<FLOAT>(gPortalWidth);
    vp.Height = static_cast<FLOAT>(gPortalHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);

    // Render the scene for the portal
    RenderSceneFromCamera(gPortalCamera);



    //// Portal 2 scene rendering ////

    // Set the portal texture and portal depth buffer as the targets for rendering
    // The portal texture will later be used on models in the main scene
    gD3DContext->OMSetRenderTargets(1, &gPortal2RenderTarget, gPortalDepthStencilView);

    // Clear the portal texture to a fixed colour and the portal depth buffer to the far distance
    gD3DContext->ClearRenderTargetView(gPortal2RenderTarget, &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gPortalDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Render the scene for the portal
    RenderSceneFromCamera(gPortal2Camera);

    //-------------------------------------------------------------------------


    //// Main scene rendering ////

    // Now set the back buffer as the target for rendering and select the main depth buffer.
    // When finished the back buffer is sent to the "front buffer" - which is the monitor.
    gD3DContext->OMSetRenderTargets(1, &gBackBufferRenderTarget, gDepthStencil);

    // Clear the back buffer to a fixed colour and the depth buffer to the far distance
    gD3DContext->ClearRenderTargetView(gBackBufferRenderTarget, &gBackgroundColor.r);
    gD3DContext->ClearDepthStencilView(gDepthStencil, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Setup the viewport to the size of the main window
    vp.Width  = static_cast<FLOAT>(gViewportWidth);
    vp.Height = static_cast<FLOAT>(gViewportHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gD3DContext->RSSetViewports(1, &vp);

    // Render the scene for the main window
    RenderSceneFromCamera(gCamera);


    //-------------------------------------------------------------------------


    //// Scene completion ////

    // When drawing to the off-screen back buffer is complete, we "present" the image to the front buffer (the screen)
    // Set first parameter to 1 to lock to vsync (typically 60fps)
    gSwapChain->Present(lockFPS ? 1 : 0, 0);
}


//--------------------------------------------------------------------------------------
// Scene Update
//--------------------------------------------------------------------------------------

// Update models and camera. frameTime is the time passed since the last frame
void UpdateScene(float frameTime)
{
	// Control sphere (will update its world matrix)
	gPortal->Control(frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma );
    //gSphere->Control(frameTime, Key_I, Key_K, Key_J, Key_L, Key_U, Key_O, Key_Period, Key_Comma);

    // Orbit the light - a bit of a cheat with the static variable [ask the tutor if you want to know what this is]
	static float rotate = 0.0f;
	gLight1->SetPosition( gCube->Position() + CVector3{ cos(rotate) * gLightOrbit, 0.0f, sin(rotate) * gLightOrbit } );
	rotate -= gLightOrbitSpeed * frameTime;

	// Control camera (will update its view matrix)
	gCamera->Control(frameTime, Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D );

    // Control camera (will update its view matrix)
    gPortalCamera->Control(frameTime, Key_T, Key_G, Key_F, Key_H, Key_B, Key_N, Key_R, Key_Y);


    // Toggle FPS limiting
    if (KeyHit(Key_P))  lockFPS = !lockFPS;

    // Show frame time / FPS in the window title //
    const float fpsUpdateTime = 0.5f; // How long between updates (in seconds)
    static float totalFrameTime = 0;
    static int frameCount = 0;
    totalFrameTime += frameTime;
    ++frameCount;
    if (totalFrameTime > fpsUpdateTime)
    {
        // Displays FPS rounded to nearest int, and frame time (more useful for developers) in milliseconds to 2 decimal places
        float avgFrameTime = totalFrameTime / frameCount;
        std::ostringstream frameTimeMs;
        frameTimeMs.precision(2);
        frameTimeMs << std::fixed << avgFrameTime * 1000;
        std::string windowTitle = "CO2409 Week 18: Render to Texture - Frame Time: " + frameTimeMs.str() +
                                  "ms, FPS: " + std::to_string(static_cast<int>(1 / avgFrameTime + 0.5f));
        SetWindowTextA(gHWnd, windowTitle.c_str());
        totalFrameTime = 0;
        frameCount = 0;
    }
}
