#include <iostream>
#include <thread>
#include <filesystem>

#include <glad/glad.h> // OpenGL functions
#include <GLFW/glfw3.h>// Window functions

// Matrix/Vector math library
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Custom headers
#include <world/chunk.h>
#include <world/world.h>
#include <util/camera.h>
#include <vfx/shader.h>
#include <vfx/textures.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// settings
float SCR_WIDTH = 1920; // Screen width
float SCR_HEIGHT = 1080; // Screen height
bool wireframe = false;
bool mouse_locked = true;

// camera
Camera camera = Camera(glm::vec3(0.0f, 5.0f, 0.0f));

// timing
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame

int main() {

    // Window creation
    // ===================================================================================

    // Initialize the GLFW window
    glfwInit();
    // Request 3.3 core (matches the shipped shaders). Avoid 4.6 — many Mesa
    // drivers (including llvmpipe) only expose up to 4.5 and window creation fails.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Use the core profile
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create the window, 
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Minecraft C++", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Tell GLFW to make the context of the window the main context on the current thread
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable VSync

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialize GLAD. GLAD manages function pointers for OpenGL to ensure that the correct function pointers are loaded
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Set the callback functions
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Set the viewport size
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    // Configure global OpenGL state
    glClearColor(0.3569f, 0.6471f, 0.7725f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    // create the shader program
    Shader shaderProgram("vfx/shaders/3.3.vertex.glsl", "vfx/shaders/3.3.fragment.glsl");

    shaderProgram.use();

    // Load the textures
    loadTextures();

    // Variables to keep track of the number of frames rendered and the total time elapsed
    int frameCount = 0;
    double totalTime = 0.0;

    // Set the global world pointer
    World::world = new World(&shaderProgram);

    // Render loop
    while(!glfwWindowShouldClose(window))
    {   
        // Calculate the delta time between frames
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Increment the frame count and total time
        frameCount++;
        totalTime += deltaTime;

        // If one second has passed, print the FPS and reset the frame count and total time
        if (totalTime >= 1.0)
        {
            std::cout << "FPS: " << frameCount << std::endl;
            frameCount = 0;
            totalTime -= 1.0;
        }

        // Process input
        processInput(window);

        // Render commands will go here
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use the shader program
        shaderProgram.use();

        // Get the view matrix
        glm::mat4 view;
        view = camera.GetViewMatrix();

        // Get the projection matrix
        glm::mat4 projection = glm::mat4(1.0f);
        projection = glm::perspective(glm::radians(camera.Zoom), float(SCR_WIDTH) / float(SCR_HEIGHT), 0.1f, 1000.0f); 

        // Pass the matrices to the shader
        shaderProgram.setMat4("projection", projection);
        shaderProgram.setMat4("view", view);

        // Load and render the chunks
        World::world->Update(camera.Position);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwPollEvents();  
        glfwSwapBuffers(window);
    }

    // Terminate GLFW
    glfwTerminate();
    return 0;
}

bool mKeyReleased = true;
bool escKeyReleased = true;


float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow *window)
{
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && escKeyReleased)
    {
        mouse_locked = !mouse_locked;
        if(mouse_locked)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else{
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            lastX = SCR_WIDTH / 2.0f;
            lastY = SCR_HEIGHT / 2.0f;
            firstMouse = true;
        }
        escKeyReleased = false;
    }
    else if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE)
        escKeyReleased = true;

    if(glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && mKeyReleased)
    {
        wireframe = !wireframe;
        if(wireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        mKeyReleased = false;
    }
    else if(glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE)
    {
        mKeyReleased = true;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.MovementSpeed = camera.DefaultSpeed * 50.0f;
    else
        camera.MovementSpeed = camera.DefaultSpeed;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if(!mouse_locked) return;

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

    camera.ProcessMouseMovement(xoffset, yoffset);
} 

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

// Callback function for when the window is resized: adjust the viewport size
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    SCR_WIDTH = width; SCR_HEIGHT = height;
    glViewport(0, 0, width, height);
}  