// FinalProject3DScene.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>         // cout, cerr
#include <cstdlib>          // EXIT_FAILURE
#include <GL/glew.h>        // GLEW library
#include <GLFW/glfw3.h>     // GLFW library
#include "meshes.h"
#include <string>
#include <sstream>

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//image loader for textures
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std; // Standard namespace

/*Shader program Macro*/
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

// Unnamed namespace
namespace
{
    const char* const WINDOW_TITLE = "Final Project 3D Scene by Farooq"; // Macro for window title

    // Variables for window width and height
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    //camera initial settings
    // camera
    glm::vec3 cameraPos = glm::vec3(0.5f, 2.5f, 18.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    bool firstMouse = true;
    bool orthoOn = false;
    glm::mat4 projection; //move to a global to set from a keystroke

    float yaw = -90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
    float pitch = 0.0f;
    float lastX = WINDOW_WIDTH / 2.0;
    float lastY = WINDOW_HEIGHT / 2.0;
    float fov = 45.0f;
    float sensitivity = 0.03f; // change this to a global with a default; to be controlled by scroll

    // timing
    float deltaTime = 0.0f;	// time between current frame and last frame
    float lastFrame = 0.0f;

    // Main GLFW window
    GLFWwindow* gWindow = nullptr;

    //Generic primative shape mesh
    Meshes meshes;

    // Shader programs
    GLuint surfaceProgramId;
    GLuint lampProgramId;

    // Texture id
    GLuint gTexture0, gTexture1, gTexture2, gTexture3, gTexture4, gTexture5, gTexture6, gTexture7;
    glm::vec2 gUVScale(1.0f, 1.0f);

    // Cube and light color
    glm::vec3 gObjectColor(0.12f, 0.69f, 0.69f);

    // Light1 position and scale
    glm::vec3 gLightPosition;
    glm::vec3 gLightScale(0.3f);

    // positions of the point lights
    glm::vec3 pointLightPositions[] = {
        glm::vec3(0.5f, 8.0f, -2.0f),
        glm::vec3(5.5f, 8.0f, -2.0f),
        glm::vec3(5.5f, 8.0f, 2.0f),
        glm::vec3(0.5f, 8.0f, 2.0f),
        glm::vec3(1.5f, 8.0f, 0.0f),
    };

    //make the lighting color global to set multiple places
    glm::vec3 coloredLightColor(0.12f, 0.69f, 0.69f); //teal
    glm::vec3 ambientLightColor(1.0f, 1.0f, 1.0f); //pure white for the Sun
    glm::vec3 pointLightColor(1.0f, 0.819f, 0.639f); //~4000k from https://andi-siess.de/rgb-to-color-temperature/
}


/* User-defined Function prototypes to:
 * initialize the program, set the window size,
 * redraw graphics on the window when resized,
 * and render graphics on the screen
 */
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);

//textures
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);

//add the new prototypes for the keys and mouse controls
void mouseCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

//lights
//function to create lights; used to make code more readable
void CreateLights();
GLint GetUniformLocation(GLuint shader, string name);


/* Vertex Shader Source Code*/
const GLchar* vertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal; // VAP position 1 for normals
layout(location = 2) in vec2 textureCoordinate;

out vec2 vertexTextureCoordinate;
out vec3 vertexNormal; // For outgoing normals to fragment shader
out vec3 vertexFragmentPos; // For outgoing color / pixels to fragment shader

//Global variables for the transform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices into clip coordinates

    vertexFragmentPos = vec3(model * vec4(position, 1.0f)); // Gets fragment / pixel position in world space only (exclude view and projection)

    vertexNormal = mat3(transpose(inverse(model))) * normal; // get normal vectors in world space only and exclude normal translation properties
    vertexTextureCoordinate = textureCoordinate;
}
);

/* Cube Fragment Shader Source Code*/
const GLchar* fragmentShaderSource = GLSL(440,
    out vec4 fragmentColor; // For outgoing cube color to the GPU

struct DirLight {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 vertexNormal; // For incoming normals
in vec3 vertexFragmentPos; // For incoming fragment position
in vec2 vertexTextureCoordinate;

// Uniform / Global variables for object color, light color, light position, and camera/view position
uniform vec3 objectColor;
uniform vec3 viewPosition;
uniform sampler2D uTexture; // Useful when working with multiple textures
uniform vec2 uvScale;
uniform float ambientStrength = 0.1f; // Set ambient or global lighting strength

uniform DirLight dirLight;
uniform PointLight pointLights[5];
uniform int materialDiffuse = 0;
uniform int materialSpecular = 1;
uniform float materialShine = 32.0f;

// function prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{
    vec3 norm = normalize(vertexNormal); // Normalize vectors to 1 unit
    vec3 viewDir = normalize(viewPosition - vertexFragmentPos);

    // phase 1: directional lighting
    vec3 result = CalcDirLight(dirLight, norm, viewDir);

    //phase 2
    for (int i = 0; i < 5; i++)
        result += CalcPointLight(pointLights[i], norm, vertexFragmentPos, viewDir);

    fragmentColor = vec4(result, 1.0); // Send lighting results to GPU
}
// calculates the color when using a directional light.
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShine);

    // combine results
    vec3 ambient = ambientStrength * light.ambient * vec3(texture(uTexture, vertexTextureCoordinate));
    vec3 diffuse = light.diffuse * diff * vec3(texture(uTexture, vertexTextureCoordinate));
    vec3 specular = light.specular * spec * vec3(texture(uTexture, vertexTextureCoordinate));

    return (ambient + diffuse + specular);
}

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), materialShine);

    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // combine results
    vec3 ambient = light.ambient * vec3(texture(uTexture, vertexTextureCoordinate));
    vec3 diffuse = light.diffuse * diff * vec3(texture(uTexture, vertexTextureCoordinate));
    vec3 specular = light.specular * spec * vec3(texture(uTexture, vertexTextureCoordinate));

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}
);

/* Lamp Shader Source Code*/
const GLchar* lampVertexShaderSource = GLSL(440,

    layout(location = 0) in vec3 position; // VAP position 0 for vertex position data
layout(location = 1) in vec4 color;  // Color data from Vertex Attrib Pointer 1

out vec4 vertexColor; // variable to transfer color data to the fragment shader

//Uniform / Global variables for the  transform matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices into clip coordinates
    vertexColor = color; // references incoming color data
}
);


/* Fragment Shader Source Code*/
const GLchar* lampFragmentShaderSource = GLSL(440,
    in vec4 vertexColor; // Variable to hold incoming color data from vertex shader

out vec4 fragmentColor; // For outgoing lamp color (smaller cube) to the GPU
uniform vec4 uObjectColor;

void main()
{
    fragmentColor = vec4(uObjectColor);
}
);

// Images are loaded with Y axis going down, but OpenGL's Y axis goes up, so let's flip it
void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}

int main(int argc, char* argv[])
{

    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    // Create the shader programs
    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, surfaceProgramId))
        return EXIT_FAILURE;

    if (!UCreateShaderProgram(lampVertexShaderSource, lampFragmentShaderSource, lampProgramId))
        return EXIT_FAILURE;

    // Create the mesh
    meshes.CreateMeshes();

    // Load texture 0
    const char* textureFilename = "Wood_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture0))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }

    // Load texture 1
    textureFilename = "NYGlobe_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture1))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }
    // Load texture 2
    textureFilename = "Base_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture2))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }
    // Load texture 3
    textureFilename = "BlackShade_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture3))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }
    // Load texture 4
    textureFilename = "HD_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture4))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }
    // Load texture 5
    textureFilename = "Switch_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture5))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }
    // Load texture 6
    textureFilename = "SwitchBack_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture6))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }
    // Load texture 7
    textureFilename = "WhiteShadow_Texture.jpg";
    if (!UCreateTexture(textureFilename, gTexture7))
    {
        cout << "Failed to load texture " << textureFilename << endl;
        return EXIT_FAILURE;
    }

    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    glUseProgram(surfaceProgramId);

    // We set the texture as texture unit 0
    glUniform1i(glGetUniformLocation(surfaceProgramId, "uTexture"), 0);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(gWindow))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        UProcessInput(gWindow);

        // Render this frame
        // Turn on wireframe mode
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); //use wireframe for QA GL_FILL for off GL_LINE for on
        URender();

        glfwPollEvents();
    }

    //delete the meshes
    meshes.DestroyMeshes();

    // Release textures
    UDestroyTexture(gTexture0);
    UDestroyTexture(gTexture1);
    UDestroyTexture(gTexture2);
    UDestroyTexture(gTexture3);
    UDestroyTexture(gTexture4);
    UDestroyTexture(gTexture5);
    UDestroyTexture(gTexture6);
    UDestroyTexture(gTexture7);
    

    // Release shader program
    UDestroyShaderProgram(surfaceProgramId);
    UDestroyShaderProgram(lampProgramId);

    exit(EXIT_SUCCESS); // Terminates the program successfully
}


// Initialize GLFW, GLEW, and create a window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    // GLFW: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW: window creation
    // ---------------------
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);

    //Register the new callbacks
    glfwSetCursorPosCallback(*window, mouseCallback);
    glfwSetScrollCallback(*window, scrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);
    glfwSetKeyCallback(*window, keyCallback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLEW: initialize
    // ----------------
    // Note: if using GLEW version 1.13 or earlier
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    // Displays GPU OpenGL version
    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void UProcessInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    //TODO
    //WASD Keys to pan Left, Tight, forward, back
    float cameraSpeed = static_cast<float>(25 * deltaTime * sensitivity);
    glm::vec3 front;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        pitch += cameraSpeed * 5;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        pitch -= cameraSpeed * 5;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        //toggle the orthographic view
        //set the ortho view then flip the global variable
        if (orthoOn) {
            orthoOn = false;
            cout << "Ortho State: " << orthoOn << endl;
        }
        else {
            orthoOn = true;
            cout << "Ortho State: " << orthoOn << endl;
        }
    }
}

// FROM: https://learnopengl.com/code_viewer_gh.php?code=src/1.getting_started/7.3.camera_mouse_zoom/camera_mouse_zoom.cpp
// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    sensitivity += 0.01 * (float)yoffset;;
    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (sensitivity > 3.0f)
        sensitivity = 3.0f;
    if (sensitivity < 0.01f)
        sensitivity = 0.01f;

    cout << sensitivity << endl;
}

// glfw: whenever the mouse moves, this callback is called
// FROM: https://learnopengl.com/code_viewer_gh.php?code=src/1.getting_started/7.3.camera_mouse_zoom/camera_mouse_zoom.cpp
// -------------------------------------------------------
void mouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top
    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

}

// glfw: handle mouse button events
// --------------------------------
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
    {
        if (action == GLFW_PRESS)
            cout << "Left mouse button pressed" << endl;
        else
            cout << "Left mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
    {
        if (action == GLFW_PRESS) {
            cout << "Middle mouse button pressed - sensitivity reset" << endl;
            sensitivity = 0.03f;
        }
        else
            cout << "Middle mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_RIGHT:
    {
        if (action == GLFW_PRESS)
            cout << "Right mouse button pressed" << endl;
        else
            cout << "Right mouse button released" << endl;
    }
    break;

    default:
        cout << "Unhandled mouse button event" << endl;
        break;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Functioned called to render a frame
void URender()
{
    glm::mat4 scale;
    glm::mat4 rotation;
    glm::mat4 translation;
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 worldView;
    GLint modelLoc;
    GLint viewLoc;
    GLint projLoc;
    GLint objectColorLoc;

    // Enable z-depth
    glEnable(GL_DEPTH_TEST);

    // Clear the frame and z buffers
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Transforms the camera: move the camera back (z axis)
    view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    if (orthoOn) {
        cameraPos = glm::vec3(0.0f, 0.0f, cameraPos.z);

        glm::vec3 front;
        yaw = -90;
        pitch = 0;

        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);

        projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f); //use the orthographic projection for QA
    }
    else
        projection = glm::perspective(glm::radians(fov), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);

    // Set the shader to be used
    glUseProgram(surfaceProgramId);

    // Retrieves and passes transform matrices to the Shader program
    modelLoc = glGetUniformLocation(surfaceProgramId, "model");
    viewLoc = glGetUniformLocation(surfaceProgramId, "view");
    projLoc = glGetUniformLocation(surfaceProgramId, "projection");
    objectColorLoc = glGetUniformLocation(surfaceProgramId, "uObjectColor");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    //Create a Plane for Desk
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(7.5f, 1.0f, 6.0f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(0.0f, 0.0f, 0.0f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    //set camera position
    glGetUniformLocation(surfaceProgramId, "cameraPos");

    //camera
    GLint viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");

    // Pass color, light, and camera data to the Cube Shader program's corresponding uniforms
    glUniform3f(objectColorLoc, gObjectColor.r, gObjectColor.g, gObjectColor.b);

    const glm::vec3 cameraPosition = cameraPos;
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    GLint UVScaleLoc = glGetUniformLocation(surfaceProgramId, "uvScale");
    glUniform2fv(UVScaleLoc, 1, glm::value_ptr(gUVScale));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture0);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    
    //Below 2 shapes will be creating a globe
    //Create Tapered Cylinder
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gTaperedCylinderMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(5.5f, 0.0f, -2.0f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture2);

    // Draws the triangles
    glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
    glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
    glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Create Sphere
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gSphereMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(5.5f, 1.2f, -2.0f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 1.0f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture1);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gSphereMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Below shape is for creating a decorative cup
    //Creates a Cylinder
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gCylinderMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(1.0f, 4.0f, 1.0f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-4.5f, 0.0f, -1.5f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 0.0f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture3);

    // Draws the triangles
    glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
    glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
    glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Another Cylinder for top of the cup
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gCylinderMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(1.1f, 0.5f, 1.1f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-4.5f, 4.0f, -1.5f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 0.0f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture3);

    // Draws the triangles
    glDrawArrays(GL_TRIANGLE_FAN, 0, 36);		//bottom
    glDrawArrays(GL_TRIANGLE_FAN, 36, 36);		//top
    glDrawArrays(GL_TRIANGLE_STRIP, 72, 146);	//sides

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Below shape is for creating Hard Drive
    // //Create a Plane for HD Cover
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(1.0f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-4.2f, 0.56f, 4.5f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture4);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);
    //Creates a Cube
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gBoxMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(2.0f, 0.5f, 3.0f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-4.2f, 0.3f, 4.5f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 0.5f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture3);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Below shapes are for Nintendo Switch Dock
    //Creates a Cube for base dock
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gBoxMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(4.5f, 0.5f, 2.0f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 2.5f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(1.0f, 0.3f, -2.5f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 0.5f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture3);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Creates a Cube for Front dock
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gBoxMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(4.5f, 2.5f, 0.2f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(1.0f, 1.8f, -1.6f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 0.5f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //Creates a Cube for Back dock
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gBoxMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(4.5f, 2.5f, 0.8f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(1.0f, 1.8f, -3.1f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glProgramUniform4f(surfaceProgramId, objectColorLoc, 0.0f, 0.5f, 0.0f, 1.0f);

    //camera
    viewPositionLoc = glGetUniformLocation(surfaceProgramId, "viewPosition");
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // //Create a Plane for Switch Front Cover
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(2.25f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(1.0f, 1.55f, -1.49f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture5);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // //Create a Plane for Switch Back Cover
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(2.25f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(1.0f, 1.55f, -3.52f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture6);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // //Create a Plane for Switch Sides
    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(0.4f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(90.0f), glm::vec3(0.0, 0.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-1.26f, 1.55f, -3.1f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(0.4f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(90.0f), glm::vec3(0.0, 0.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(3.26f, 1.55f, -3.1f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(0.1f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(90.0f), glm::vec3(0.0, 0.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-1.26f, 1.55f, -1.6f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(0.1f, 0.5f, 1.5f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(90.0f), glm::vec3(0.0, 0.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(3.26f, 1.55f, -1.6f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(0.7f, 0.1f, 0.3f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(90.0f), glm::vec3(0.0, 0.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(3.26f, 0.3f, -2.3f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    // Activate the VBOs contained within the mesh's VAO
    glBindVertexArray(meshes.gPlaneMesh.vao);

    // 1. Scales the object
    scale = glm::scale(glm::vec3(0.7f, 0.1f, 0.3f));
    // 2. Rotate the object
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    rotation = glm::rotate(glm::radians(90.0f), glm::vec3(1.0, 0.0f, 0.0f));
    rotation = glm::rotate(rotation, glm::radians(90.0f), glm::vec3(0.0, 0.0f, 1.0f));
    // 3. Position the object
    translation = glm::translate(glm::vec3(-1.254f, 0.3f, -2.3f));
    // Model matrix: transformations are applied right-to-left order
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTexture7);

    // Draws the triangles
    glDrawElements(GL_TRIANGLES, meshes.gPlaneMesh.nIndices, GL_UNSIGNED_INT, (void*)0);

    // Deactivate the Vertex Array Object
    glBindVertexArray(0);

    //End of creating Switch's sides cover
    

    //create the light casters
    CreateLights();

    //loop to draw the lamps
    int numElements = sizeof(pointLightPositions) / sizeof(pointLightPositions[0]);

    for (unsigned int i = 0; i < numElements; i++) {

        glUseProgram(lampProgramId);
        glBindVertexArray(meshes.gBoxMesh.vao);

        //Transform the smaller cube used as a visual que for the light source
        gLightPosition = pointLightPositions[i];
        model = glm::translate(gLightPosition) * glm::scale(gLightScale);

        // Reference matrix uniforms from the Lamp Shader program
        modelLoc = glGetUniformLocation(lampProgramId, "model");
        viewLoc = glGetUniformLocation(lampProgramId, "view");
        projLoc = glGetUniformLocation(lampProgramId, "projection");
        objectColorLoc = glGetUniformLocation(lampProgramId, "uObjectColor");

        // Pass matrix data to the Lamp Shader program's matrix uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        //for the first 4 lights set to the point light color
        //for the 5th light set to the colored light
        if (i != 4) {
            glProgramUniform4f(lampProgramId, objectColorLoc, pointLightColor.r, pointLightColor.g, pointLightColor.b, 1.0f);
        }
        else {
            glProgramUniform4f(lampProgramId, objectColorLoc, coloredLightColor.r, coloredLightColor.g, coloredLightColor.b, 1.0f);
        }

        // Draws the triangles
        glDrawElements(GL_TRIANGLES, meshes.gBoxMesh.nIndices, GL_UNSIGNED_INT, (void*)0);
    }

    // Deactivate the Vertex Array Object and shader program
    glBindVertexArray(0);
    glUseProgram(0);

    // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
    glfwSwapBuffers(gWindow);    // Flips the the back buffer with the front buffer every frame.
}

/*Generate and load the texture*/
bool UCreateTexture(const char* filename, GLuint& textureId)
{
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        flipImageVertically(image, width, height, channels);

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            cout << "Not implemented to handle image with " << channels << " channels" << endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

        return true;
    }

    // Error loading the image
    return false;
}

void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}

// Implements the UCreateShaders function
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
    // Compilation and linkage error reporting
    int success = 0;
    char infoLog[512];

    // Create a Shader program object.
    programId = glCreateProgram();

    // Create the vertex and fragment shader objects
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    // Retrive the shader source
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    // Compile the vertex shader, and print compilation errors (if any)
    glCompileShader(vertexShaderId); // compile the vertex shader
    // check for shader compile errors
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glCompileShader(fragmentShaderId); // compile the fragment shader
    // check for shader compile errors
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;

        return false;
    }

    // Attached compiled shaders to the shader program
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);   // links the shader program
    // check for linking errors
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;

        return false;
    }

    glUseProgram(programId);    // Uses the shader program

    return true;
}


void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}


//Helper function that takes a string
GLint GetUniformLocation(GLuint shader, string name)
{
    return glGetUniformLocation(shader, name.c_str());
}

void CreateLights()
{
    //ambient light
    glm::vec3 ambientLightPos(-6.0f, -4.0f, 0.0f);

    glUniform3f(glGetUniformLocation(surfaceProgramId, "dirLight.direction"), ambientLightPos.x, ambientLightPos.y, ambientLightPos.z);
    glUniform3f(glGetUniformLocation(surfaceProgramId, "dirLight.ambient"), ambientLightColor.r, ambientLightColor.g, ambientLightColor.b);
    glUniform3f(glGetUniformLocation(surfaceProgramId, "dirLight.diffuse"), ambientLightColor.r, ambientLightColor.g, ambientLightColor.b);
    glUniform3f(glGetUniformLocation(surfaceProgramId, "dirLight.specular"), ambientLightColor.r, ambientLightColor.g, ambientLightColor.b);


    //loop throught the pointLights and give them a definition
    int numElements = sizeof(pointLightPositions) / sizeof(pointLightPositions[0]);
    for (unsigned int i = 0; i < numElements; i++) {

        glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].position"), pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);

        //make the 5th light in the array colored
        if (i != 4) {
            glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].ambient"), pointLightColor.r, pointLightColor.g, pointLightColor.b);
            glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].diffuse"), pointLightColor.r, pointLightColor.g, pointLightColor.b);
            glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].specular"), pointLightColor.r, pointLightColor.g, ambientLightColor.b);

            //set the attenuation components to ~20 units
            //from: https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
            glUniform1f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].constant"), 1.0f);
            glUniform1f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].linear"), 0.22f);
            glUniform1f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].quadratic"), 0.20f);
        }
        else {
            glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].ambient"), coloredLightColor.r, coloredLightColor.g, coloredLightColor.b);
            glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].diffuse"), coloredLightColor.r, coloredLightColor.g, coloredLightColor.b);
            glUniform3f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].specular"), coloredLightColor.r, coloredLightColor.g, coloredLightColor.b);

            //set the attenuation components to ~50 units to make the color cast stronger
            //from: https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
            glUniform1f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].constant"), 1.0f);
            glUniform1f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].linear"), 0.09f);
            glUniform1f(GetUniformLocation(surfaceProgramId, "pointLights[" + std::to_string(i) + "].quadratic"), 0.032f);
        }

    }
}
