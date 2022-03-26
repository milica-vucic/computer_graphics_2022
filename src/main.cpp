#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadCubemap(vector<std::string> faces);
unsigned int loadTexture(const char* path, bool gammaCorrection);

// settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 650;

// camera

float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool spotlightEnabled = true;
bool blinn = true;

struct DirLight {
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    bool hdr = false;
    bool bloom = false;
    float exposure = 0.197f;
    float gamma = 2.2f;
    int kernelEffects = 3;
    DirLight dirLight;
    ProgramState()
            : camera(glm::vec3(-10.36f, -2.63f, 36.34f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << ImGuiEnabled << '\n'
        << bloom << "\n"
        << exposure << "\n"
        << hdr << "\n"
        << gamma << "\n"
        << kernelEffects << "\n"
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> ImGuiEnabled
           >> bloom
           >> exposure
           >> hdr
           >> gamma
           >> kernelEffects
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);
void setNightLights(Shader& shader, float currentFrame);
void setDayLights(Shader& shader, float currentFrame);
void renderQuad();

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "aztec temple", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
//    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;



    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // build and compile shaders
    // -------------------------
    Shader objectShader("resources/shaders/model_lighting.vs", "resources/shaders/model_lighting.fs");
    Shader skyboxShader("resources/shaders/skybox_shader.vs", "resources/shaders/skybox_shader.fs");
    Shader discardShader("resources/shaders/discard_shader.vs", "resources/shaders/discard_shader.fs");
    Shader instanceShader("resources/shaders/instance_shader.vs", "resources/shaders/instance_shader.fs");
    Shader screenShader("resources/shaders/framebuffers.vs", "resources/shaders/framebuffers.fs");
    Shader blurShader("resources/shaders/blur.vs", "resources/shaders/blur.fs");

    // load models
    // -----------
    Model temple("resources/objects/temple/temple.obj");
    temple.SetShaderTextureNamePrefix("material.");
    Model terrain("resources/objects/terrain/terrain.obj");
    terrain.SetShaderTextureNamePrefix("material.");
    Model moon("resources/objects/moon/model.obj");
    moon.SetShaderTextureNamePrefix("material.");
    Model totem("resources/objects/totem/totem.obj");
    totem.SetShaderTextureNamePrefix("material.");
    Model tree("resources/objects/tree/CoconutPalm.obj");
    tree.SetShaderTextureNamePrefix("material.");

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    float transparentVertices[] = {
            // positions         // texture Coords
            0.0f, -0.5f,  0.0f,  0.0f,  0.0f,
            0.0f,  0.5f,  0.0f,  0.0f,  1.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  1.0f,

            0.0f, -0.5f,  0.0f,  0.0f,  0.0f,
            1.0f,  0.5f,  0.0f,  1.0f,  1.0f,
            1.0f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    std::vector<glm::vec3> vegetation = {
            glm::vec3(-20.7f, -6.73f, 10.4f),
            glm::vec3(-24.5f, -6.73f, 9.8f),
            glm::vec3(-28.3f, -6.73f, 10.4f),
            glm::vec3(-32.1f, -6.73, 9.8f),
            glm::vec3(-5.0f, -6.87f, 10.4f),
            glm::vec3(-1.8f, -6.87f, 9.8f),
            glm::vec3(2.0f, -6.87, 10.4f),
            glm::vec3(5.8f, -6.87, 9.8f)
    };

    std::vector<std::string> faces_night = {
            "resources/textures/skybox/night/right.png",
            "resources/textures/skybox/night/left.png",
            "resources/textures/skybox/night/top.png",
            "resources/textures/skybox/night/bottom.png",
            "resources/textures/skybox/night/front.png",
            "resources/textures/skybox/night/back.png"
    };

    // -------- Skybox --------
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    unsigned int transparentVAO, transparentVBO;
    glGenVertexArrays(1, &transparentVAO);
    glGenBuffers(1, &transparentVBO);

    glBindVertexArray(transparentVAO);
    glBindBuffer(GL_ARRAY_BUFFER, transparentVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transparentVertices), &transparentVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // -------- Framebuffers setup --------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer is not complete!" << "\n";

    // ping-pong framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "ERROR::FRAMEBUFFER:: Pingpong Framebuffer not complete!" << std::endl;
    }

    // ------ Shader configuration ------
    skyboxShader.use();
    unsigned int skyboxTexture = loadCubemap(faces_night);
    skyboxShader.setInt("skybox", 0);

    unsigned int grassTexture = loadTexture(FileSystem::getPath("resources/textures/grass.png").c_str(), true);
    discardShader.use();
    discardShader.setInt("texture0", 0);

    objectShader.use();
    objectShader.setInt("texture_diffuse1", 0);
    objectShader.setInt("texture_specular1", 1);
    blurShader.use();
    blurShader.setInt("image", 0);

    // start values for directional light
    programState->dirLight.direction = glm::vec3(-21.882572f, -35.517292f, -37.401550f);
    programState->dirLight.ambient = glm::vec3(-0.3f, 0.2f, 0.3f);
    programState->dirLight.diffuse = glm::vec3(0.3f, 0.2f, 0.3f);
    programState->dirLight.specular = glm::vec3(0.2f, 0.1f, 0.3f);

    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // render
        // ------
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // don't forget to enable shader before setting uniforms
        objectShader.use();
        objectShader.setBool("blinn", blinn);

        setNightLights(objectShader, currentFrame);
        if (spotlightEnabled) {
            objectShader.setVec3("spotLight.position", programState->camera.Position);
            objectShader.setVec3("spotLight.direction", programState->camera.Front);
            objectShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(13.5f)));
            objectShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(18.5f)));
            objectShader.setFloat("spotLight.constant", 0.8f);
            objectShader.setFloat("spotLight.linear", 0.2f);
            objectShader.setFloat("spotLight.quadratic", 0.12f);
            objectShader.setVec3("spotLight.ambient", glm::vec3(1.0f, 1.0f, 1.0f));
            objectShader.setVec3("spotLight.diffuse", glm::vec3(0.8f, 0.8f, 0.8f));
            objectShader.setVec3("spotLight.specular", glm::vec3(1.0f, 1.0f, 1.0f));
        } else {
            objectShader.setVec3("spotLight.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
            objectShader.setVec3("spotLight.diffuse", glm::vec3(0.0f, 0.0f, 0.0f));
            objectShader.setVec3("spotLight.specular", glm::vec3(0.0f, 0.0f, 0.0f));
        }

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        objectShader.setMat4("projection", projection);
        objectShader.setMat4("view", view);

        // -------- Objects --------
        // terrain
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-15.0f, -12.5, -15.0f));
        model = glm::scale(model, glm::vec3(10.0f, 10.0f, 10.0f));
        objectShader.setMat4("model", model);
        terrain.Draw(objectShader);

        // temple
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-10.0f, -8.7f, -10.0f));
        model = glm::scale(model, glm::vec3(4.0f,  4.0f, 4.0f));
        objectShader.setMat4("model", model);
        temple.Draw(objectShader);

        // totem 1
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-15.0f, -8.8f, 8.9f));
        model = glm::rotate(model, glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.7f, 1.7f, 1.7f));
        objectShader.setMat4("model", model);
        totem.Draw(objectShader);

        // totem 2
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-5.0f, -8.7f, 8.9f));
        model = glm::rotate(model, glm::radians(10.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.7f, 1.7f, 1.7f));
        objectShader.setMat4("model", model);
        totem.Draw(objectShader);

        // moon
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(25.0f, 38.0f, -40.5f));
        model = glm::rotate(model, currentFrame / 3.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(5.0f, 5.0f, 5.0f));
        objectShader.setMat4("model", model);
        moon.Draw(objectShader);
        glDisable(GL_CULL_FACE);

        // palm trees
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-29.108009f, -7.468780f, -23.254124f));
        model = glm::rotate(model, glm::radians(-50.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.1f));
        objectShader.setMat4("model", model);
        tree.Draw(objectShader);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(10.238466f, -7.468780f, -23.254124f));
        model = glm::rotate(model, glm::radians(-64.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.1f));
        objectShader.setMat4("model", model);
        tree.Draw(objectShader);

        // grass
        discardShader.use();
        glBindVertexArray(transparentVAO);
        discardShader.setMat4("view", view);
        discardShader.setMat4("projection", projection);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        for (unsigned int i = 0; i < vegetation.size(); ++i) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, vegetation[i]);
            model = glm::scale(model, glm::vec3(4.0f));
            discardShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
        skyboxShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix())); // remove translation from the view matrix
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // blur
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 5;
        blurShader.use();
        for (unsigned int i = 0; i < amount; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render the quad plane on default framebuffer
        screenShader.use();
        screenShader.setInt("bloom", programState->bloom);
        screenShader.setInt("effect", programState->kernelEffects);
        screenShader.setInt("hdr", programState->hdr);
        screenShader.setFloat("exposure", programState->exposure);
        screenShader.setFloat("gamma", programState->gamma);
        // Bind bloom and non bloom
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);

        renderQuad();

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glDeleteVertexArrays(1, &transparentVAO);
    glDeleteVertexArrays(1, &skyboxVAO);

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (programState->camera.Position.z < 38.942f) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(RIGHT, deltaTime);
    } else {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            programState->camera.ProcessKeyboard(RIGHT, deltaTime);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiStyle* style = &ImGui::GetStyle();
    style->FrameRounding = 7.0f;

    {
        static float f = 0.0f;
        ImGui::Begin("Settings");

        ImGui::Text("Hdr/Bloom");
        ImGui::Checkbox("HDR", &programState->hdr);
        if (programState->hdr) {
            ImGui::Checkbox("Bloom", &programState->bloom);
            ImGui::DragFloat("Exposure", &programState->exposure, 0.05f, 0.0f, 5.0f);
            ImGui::DragFloat("Gamma factor", &programState->gamma, 0.05f, 0.0f, 4.0f);
        }

        ImGui::Text("Kernel effects");
        ImGui::RadioButton("Blur", &programState->kernelEffects, 0);
        ImGui::RadioButton("Grayscale", &programState->kernelEffects, 1);
        ImGui::RadioButton("Edge detection", &programState->kernelEffects, 2);
        ImGui::RadioButton("None", &programState->kernelEffects, 3);

        ImGui::Text("Directional light adjustment");
        ImGui::DragFloat3("Direction", (float*)&programState->dirLight.direction, 0.05, -10.0f, 10.0f, "%.4f", 0);
        ImGui::DragFloat3("Ambient", (float*)&programState->dirLight.ambient, 0.02f, -1.0f, 1.0f, "%.4f", 0);
        ImGui::DragFloat3("Diffuse", (float*)&programState->dirLight.diffuse, 0.02f, -1.0f, 1.0f, "%.4f", 0);
        ImGui::DragFloat3("Specular", (float*)&programState->dirLight.specular, 0.02f, -1.0f, .0f, "%.4f", 0);

        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        spotlightEnabled = !spotlightEnabled;
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        blinn = !blinn;
    }
}

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    stbi_set_flip_vertically_on_load(true);
    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, internalFormat == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, internalFormat == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void setNightLights(Shader& shader, float currentFrame)
{
    shader.setVec3("dirLight.direction", programState->dirLight.direction);
    shader.setVec3("dirLight.ambient", programState->dirLight.ambient);
    shader.setVec3("dirLight.diffuse", programState->dirLight.diffuse);
    shader.setVec3("dirLight.specular", programState->dirLight.specular);

    // point lights
    shader.setVec3("pointLights[0].position", glm::vec3(-10.007275f, 4.587323f, -9.562702));
    shader.setVec3("pointLights[0].ambient", glm::vec3(3.0f, 0.0f, 0.0f) * cos(currentFrame));
    shader.setVec3("pointLights[0].diffuse", glm::vec3(3.0f, 0.0f, 0.0f) * sin(currentFrame));
    shader.setVec3("pointLights[0].specular", glm::vec3(3.0f, 0.0f, 0.0f));
    shader.setFloat("pointLights[0].constant", 0.783f);
    shader.setFloat("pointLights[0].linear", 0.21f);
    shader.setFloat("pointLights[0].quadratic", 0.045f);
    shader.setFloat("material.shininess", 8.0f);

    shader.setVec3("pointLights[1].position", glm::vec3(21.882572f, 35.517292f, -37.401550f));
    shader.setVec3("pointLights[1].ambient", glm::vec3(10.0f, 10.0f, 10.0f));
    shader.setVec3("pointLights[1].diffuse", glm::vec3(10.0f, 10.0f, 10.0f));
    shader.setVec3("pointLights[1].specular", glm::vec3(3.0f, 3.0f, 3.0f));
    shader.setFloat("pointLights[1].constant", 0.45f);
    shader.setFloat("pointLights[1].linear", 0.54f);
    shader.setFloat("pointLights[1].quadratic", 0.78f);
    shader.setFloat("material.shininess", 64.0f);

    std::vector<glm::vec3> positions = {
            glm::vec3(-11.023065f, 8.9135011f * cos(currentFrame / 1.5f), 14.310511f * sin(currentFrame / 1.5f)),
            glm::vec3(12.824927f, 8.9135011f * cos(currentFrame * 1.34f), -6.830830f * sin(currentFrame * 1.34f)),
            glm::vec3(-9.759034f, 8.9135011f * cos(currentFrame / 1.5f), -30.399181f * sin(currentFrame / 1.5f)),
            glm::vec3(-34.675980f, 8.9135011f * cos(currentFrame * 1.34f), -8.309442f * sin(currentFrame / 1.34f))
    };

    for (unsigned int i = 0; i < 4; ++i) {
        shader.setVec3("pointLights[" + std::to_string(i + 2) + "].position", positions[i]);
        shader.setVec3("pointLights[" + std::to_string(i + 2) + "].ambient", glm::vec3(1.5f, 1.5f, 1.5f));
        shader.setVec3("pointLights[" + std::to_string(i + 2) + "].diffuse", glm::vec3(0.9f, 0.9f, 0.9f));
        shader.setVec3("pointLights[" + std::to_string(i + 2) + "].specular", glm::vec3(0.5f, 0.5f, 0.5f));
        shader.setFloat("pointLights[" + std::to_string(i + 2) + "].constant", 0.9f);
        shader.setFloat("pointLights[" + std::to_string(i + 2) + "].linear", 0.47f);
        shader.setFloat("pointLights[" + std::to_string(i + 2) + "].quadratic", 0.024f);
        shader.setFloat("material.shininess", 64.0f);
    }
}