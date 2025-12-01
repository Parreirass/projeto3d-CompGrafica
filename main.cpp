#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <random>
#include <chrono>
#include <map>
#include <algorithm> // Necessário para std::max/std::min

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp> // Para debug

// --- CONFIGURAÇÕES GLOBAIS ---
const int WIDTH = 800;
const int HEIGHT = 600;
const float PI = 3.14159265359f;

// --- Estados do Jogo ---
enum GameState { STATE_READY, STATE_RUNNING_UP, STATE_KICKING, STATE_BALL_IN_FLIGHT, STATE_SAVED, STATE_GOAL, STATE_RESETTING, STATE_GAMEOVER, STATE_CELEBRATING};
GameState g_gameState = STATE_READY;
enum KeeperState { KEEPER_IDLE, KEEPER_DIVING };
KeeperState g_keeperState = KEEPER_IDLE;
enum Team { TEAM_1, TEAM_2 };
Team g_currentKicker = TEAM_1;
const glm::vec4 g_team1Color1(0.1f, 0.1f, 0.1f, 1.0f); const glm::vec4 g_team1Color2(0.9f, 0.9f, 0.9f, 1.0f);
const glm::vec4 g_team2Color1(0.0f, 0.2f, 0.8f, 1.0f); const glm::vec4 g_team2Color2(0.0f, 0.2f, 0.8f, 1.0f);
const glm::vec4 g_keeperColor(1.0f, 1.0f, 0.2f, 1.0f);
const glm::vec4 g_shortsColor(0.9f, 0.9f, 0.9f, 1.0f);
const glm::vec4 g_skinColor(1.0f, 0.8f, 0.6f, 1.0f);
const glm::vec4 g_gloveColor(0.1f, 0.1f, 0.1f, 1.0f); // NOVO: Cor Preta para Luvas

// --- Lógica do Placar ---
int g_currentKick = 0;
std::vector<int> g_team1Results = {0, 0, 0};
std::vector<int> g_team2Results = {0, 0, 0};

// --- Posições e Dimensões ---
glm::vec3 g_playerStartPos(2.0f, 1.1f, 8.0f);
glm::vec3 g_playerPosition(g_playerStartPos);
glm::vec3 g_ballPosition(0.0f, 0.1f, 6.0f);
glm::vec3 g_keeperPosition(0.0f, 0.8f, -10.0f);

const glm::vec3 g_playerTorsoSize(0.5f, 0.7f, 0.3f);
const glm::vec3 g_playerLimbSize(0.15f, 0.35f, 0.15f);
const float g_playerHeadRadius = 0.2f;
const glm::vec3 g_keeperTorsoSize(0.5f, 0.7f, 0.3f);
const glm::vec3 g_keeperLimbSize(0.15f, 0.4f, 0.15f);
const float g_keeperHeadRadius = 0.2f;
const float g_ballRadius = 0.1f;
const float g_goalWidth = 4.0f;
const float g_goalHeight = 2.0f;
const glm::vec3 g_goalPostSize(0.2f, 2.0f, 0.2f); // Ainda usado para colisões, pode ser removido se colisão for com cilindro

// --- NOVOS: parâmetros do gol / rede ---
const float g_goalLineZ = -10.0f;
const float g_netDepth = 1.5f;
const float g_backNetZ = g_goalLineZ - g_netDepth;
bool g_goalRecorded = false;

// --- Física, IA e Animação ---
glm::vec3 g_ballVelocity(0.0f);
glm::vec3 g_keeperTargetPos(g_keeperPosition);
float g_ballSpeed = 15.0f;
float g_keeperDiveSpeed = 3.0f;
float g_playerRunSpeed = 3.0f;
int   g_kickRequest = 0;
float g_resetTimer = 0.0f;
float g_netAnimationTimer = 0.0f;
float g_reboundTimer = 0.0f;
float g_animationTimer = 0.0f;

std::default_random_engine g_randomEngine(std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_int_distribution<int> g_keeperChoice(1, 3);

glm::vec3 g_lightPos(0.0f, 5.0f, 5.0f);
//glm::vec3 g_cameraPos(-11.0f, 6.0f, 17.0f); // X=8 (Direita), Y=6 (Alto), Z=10 (Um pouco mais perto)
// Ponto que a câmera sempre orbitará (a origem, no seu caso)
glm::vec3 g_cameraTarget = glm::vec3(0.0f, 1.0f, 0.0f); // 1m acima da origem

// Variáveis da câmera orbital (coordenadas esféricas)
float g_cameraRadius = 20.0f; // Distância (Zoom). Começa com 20.0f
float g_cameraYaw = glm::radians(45.0f);   // Rotação horizontal
float g_cameraPitch = glm::radians(20.0f); // Rotação vertical

// Variáveis dinâmicas (serão calculadas a cada frame)
glm::vec3 g_cameraPos;  // Posição da câmera (calculada)
glm::mat4 g_viewMatrix; // Matriz view (calculada)

// Variáveis para controle do mouse
bool g_isMouseDragging = false;
double g_lastMouseX, g_lastMouseY;

// --- SHADERS (Iluminação) ---
const char* lightingVertexShader = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "uniform mat4 model;\n uniform mat4 view;\n uniform mat4 projection;\n"
    "out vec3 FragPos;\n out vec3 Normal;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "   FragPos = vec3(model * vec4(aPos, 1.0));\n"
    "   Normal = mat3(transpose(inverse(model))) * aNormal;\n"
    "}\0";
const char* lightingFragmentShader = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 FragPos;\n in vec3 Normal;\n"
    "uniform vec4 objectColor;\n uniform vec3 lightPos;\n uniform vec3 viewPos;\n"
    "void main()\n"
    "{\n"
    "   float ambientStrength = 0.3;\n"
    "   vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);\n"
    "   vec3 norm = normalize(Normal);\n"
    "   vec3 lightDir = normalize(lightPos - FragPos);\n"
    "   float diff = max(dot(norm, lightDir), 0.0);\n"
    "   vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);\n"
    "   float specularStrength = 0.5;\n"
    "   vec3 viewDir = normalize(viewPos - FragPos);\n"
    "   vec3 reflectDir = reflect(-lightDir, norm);\n"
    "   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
    "   vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);\n"
    "   vec3 result = (ambient + diffuse + specular) * objectColor.rgb;\n"
    "   FragColor = vec4(result, objectColor.a);\n"
    "}\n\0";

/**
 * (NOVA) Calcula g_cameraPos e g_viewMatrix com base nos ângulos
 */
void updateCamera() {
    // 1. Trava o ângulo vertical (pitch) para não virar de cabeça para baixo
    g_cameraPitch = glm::clamp(g_cameraPitch, glm::radians(5.0f), glm::radians(89.0f));

    // 2. Calcula a nova posição 3D da câmera
    g_cameraPos.x = g_cameraTarget.x + g_cameraRadius * cos(g_cameraPitch) * cos(g_cameraYaw);
    g_cameraPos.y = g_cameraTarget.y + g_cameraRadius * sin(g_cameraPitch);
    g_cameraPos.z = g_cameraTarget.z + g_cameraRadius * cos(g_cameraPitch) * sin(g_cameraYaw);

    // 3. Recalcula a View Matrix (como você fazia, mas com as vars dinâmicas)
    g_viewMatrix = glm::lookAt(g_cameraPos, g_cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

/**
 * (NOVA) Processa input do teclado (setinhas)
 */
void processInput(GLFWwindow* window) {
    float cameraSpeed = 0.03f; // Sensibilidade da rotação

    // Rotaciona para Esquerda
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        g_cameraYaw -= cameraSpeed;
    // Rotaciona para Direita
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        g_cameraYaw += cameraSpeed;
    // Rotaciona para Cima
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        g_cameraPitch += cameraSpeed;
    // Rotaciona para Baixo
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        g_cameraPitch -= cameraSpeed;
}

/**
 * (NOVA) Callback para o clique do mouse
 */
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_isMouseDragging = true;
            glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
        } else if (action == GLFW_RELEASE) {
            g_isMouseDragging = false;
        }
    }
}

/**
 * (NOVA) Callback para a posição do mouse (arrastar)
 */
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!g_isMouseDragging) return;

    float xOffset = xpos - g_lastMouseX;
    float yOffset = g_lastMouseY - ypos; // Invertido

    g_lastMouseX = xpos;
    g_lastMouseY = ypos;

    float sensitivity = 0.005f;
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    g_cameraYaw += xOffset;
    g_cameraPitch += yOffset;
}

/**
 * (NOVA) Callback para o scroll do mouse (zoom)
 */
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    float zoomSpeed = 0.5f;
    g_cameraRadius -= yoffset * zoomSpeed;
    g_cameraRadius = glm::clamp(g_cameraRadius, 3.0f, 25.0f); // Limita o zoom
}

// --- GEOMETRIA ---
float cubeVerticesNormals[] = { -0.5f,-0.5f,-0.5f, 0.0f, 0.0f,-1.0f, 0.5f,-0.5f,-0.5f, 0.0f, 0.0f,-1.0f, 0.5f, 0.5f,-0.5f, 0.0f, 0.0f,-1.0f, 0.5f, 0.5f,-0.5f, 0.0f, 0.0f,-1.0f,-0.5f, 0.5f,-0.5f, 0.0f, 0.0f,-1.0f,-0.5f,-0.5f,-0.5f, 0.0f, 0.0f,-1.0f,-0.5f,-0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f,-0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,-0.5f,-0.5f, 0.5f, 0.0f, 0.0f, 1.0f,-0.5f, 0.5f, 0.5f,-1.0f, 0.0f, 0.0f,-0.5f, 0.5f,-0.5f,-1.0f, 0.0f, 0.0f,-0.5f,-0.5f,-0.5f,-1.0f, 0.0f, 0.0f,-0.5f,-0.5f,-0.5f,-1.0f, 0.0f, 0.0f,-0.5f,-0.5f, 0.5f,-1.0f, 0.0f, 0.0f,-0.5f, 0.5f, 0.5f,-1.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f,-0.5f, 1.0f, 0.0f, 0.0f, 0.5f,-0.5f,-0.5f, 1.0f, 0.0f, 0.0f, 0.5f,-0.5f,-0.5f, 1.0f, 0.0f, 0.0f, 0.5f,-0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,-0.5f,-0.5f,-0.5f, 0.0f,-1.0f, 0.0f, 0.5f,-0.5f,-0.5f, 0.0f,-1.0f, 0.0f, 0.5f,-0.5f, 0.5f, 0.0f,-1.0f, 0.0f, 0.5f,-0.5f, 0.5f, 0.0f,-1.0f, 0.0f,-0.5f,-0.5f, 0.5f, 0.0f,-1.0f, 0.0f,-0.5f,-0.5f,-0.5f, 0.0f,-1.0f, 0.0f,-0.5f, 0.5f,-0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f,-0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,-0.5f, 0.5f,-0.5f, 0.0f, 1.0f, 0.0f };
unsigned int createCubeVAO() {
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerticesNormals), cubeVerticesNormals, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return VAO;
}
std::pair<unsigned int, int> createSphereVAO(float radius, int sectors, int stacks) {
    std::vector<float> vertices; std::vector<unsigned int> indices;
    float x, y, z, xy, nx, ny, nz, lengthInv = 1.0f / radius;
    float stackStep = PI / stacks, sectorStep = 2 * PI / sectors, stackAngle, sectorAngle;
    for (int i = 0; i <= stacks; ++i) {
        stackAngle = PI / 2 - i * stackStep; xy = radius * cos(stackAngle); z = radius * sin(stackAngle);
        for (int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep; x = xy * cos(sectorAngle); y = xy * sin(sectorAngle);
            vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
            nx = x * lengthInv; ny = y * lengthInv; nz = z * lengthInv;
            vertices.push_back(nx); vertices.push_back(ny); vertices.push_back(nz);
        }
    }
    int k1, k2;
    for (int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1); k2 = k1 + sectors + 1;
        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) { indices.push_back(k1); indices.push_back(k2); indices.push_back(k1 + 1); }
            if (i != (stacks - 1)) { indices.push_back(k1 + 1); indices.push_back(k2); indices.push_back(k2 + 1); }
        }
    }
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return {VAO, (int)indices.size()};
}
// NOVO: FUNÇÃO PARA CRIAR CILINDRO
std::pair<unsigned int, int> createCylinderVAO(float radius, float height, int sectors) {
    std::vector<float> vertices; std::vector<unsigned int> indices;
    float halfHeight = height / 2.0f; float sectorStep = 2 * PI / sectors; float sectorAngle;
    for (int i = 0; i <= sectors; ++i) {
        sectorAngle = i * sectorStep; float x = radius * cos(sectorAngle); float z = radius * sin(sectorAngle);
        vertices.push_back(x); vertices.push_back( halfHeight); vertices.push_back(z); vertices.push_back(x/radius); vertices.push_back(0.0f); vertices.push_back(z/radius);
        vertices.push_back(x); vertices.push_back(-halfHeight); vertices.push_back(z); vertices.push_back(x/radius); vertices.push_back(0.0f); vertices.push_back(z/radius);
    }
    int topCenterIndex = vertices.size() / 6;
    vertices.push_back(0.0f); vertices.push_back( halfHeight); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back( 1.0f); vertices.push_back(0.0f);
    int bottomCenterIndex = vertices.size() / 6;
    vertices.push_back(0.0f); vertices.push_back(-halfHeight); vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(-1.0f); vertices.push_back(0.0f);
    int topCapStartIndex = vertices.size() / 6;
     for (int i = 0; i <= sectors; ++i) {
        sectorAngle = i * sectorStep; float x = radius * cos(sectorAngle); float z = radius * sin(sectorAngle);
        vertices.push_back(x); vertices.push_back( halfHeight); vertices.push_back(z); vertices.push_back(0.0f); vertices.push_back( 1.0f); vertices.push_back(0.0f);
     }
    int bottomCapStartIndex = vertices.size() / 6;
     for (int i = 0; i <= sectors; ++i) {
        sectorAngle = i * sectorStep; float x = radius * cos(sectorAngle); float z = radius * sin(sectorAngle);
        vertices.push_back(x); vertices.push_back(-halfHeight); vertices.push_back(z); vertices.push_back(0.0f); vertices.push_back(-1.0f); vertices.push_back(0.0f);
     }
    for (int i = 0; i < sectors; ++i) {
        int i0 = i*2, i1 = i*2+1, i2 = (i+1)*2, i3 = (i+1)*2+1;
        indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
        indices.push_back(i2); indices.push_back(i1); indices.push_back(i3);
    }
    for (int i = 0; i < sectors; ++i) { indices.push_back(topCenterIndex); indices.push_back(topCapStartIndex + i); indices.push_back(topCapStartIndex + i + 1); }
    for (int i = 0; i < sectors; ++i) { indices.push_back(bottomCenterIndex); indices.push_back(bottomCapStartIndex + i + 1); indices.push_back(bottomCapStartIndex + i); }
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return {VAO, (int)indices.size()};
}
// --- FIM GEOMETRIA ---


// --- FUNÇÕES DE DESENHO BASE ---
void drawCube(unsigned int shaderProgram, glm::mat4 model, glm::vec4 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(glGetUniformLocation(shaderProgram, "objectColor"), color.r, color.g, color.b, color.a);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}
void drawSphere(unsigned int shaderProgram, unsigned int sphereVAO, int indexCount, glm::mat4 model, glm::vec4 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(glGetUniformLocation(shaderProgram, "objectColor"), color.r, color.g, color.b, color.a);
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}
// NOVO: FUNÇÃO PARA DESENHAR CILINDRO
void drawCylinder(unsigned int shaderProgram, unsigned int cylinderVAO, int indexCount, glm::mat4 model, float height, float radius, glm::vec4 color) {
    model = glm::scale(model, glm::vec3(radius, height, radius));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(glGetUniformLocation(shaderProgram, "objectColor"), color.r, color.g, color.b, color.a);
    glBindVertexArray(cylinderVAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

void drawPlayer(unsigned int shaderProgram, unsigned int cubeVAO, unsigned int sphereVAO, int sphereIndexCount, glm::vec3 position, Team team) {
    glm::mat4 baseTransform = glm::translate(glm::mat4(1.0f), position);
    glm::vec3 direction = g_ballPosition - position;
    float angleY = atan2(direction.x, direction.z);
    baseTransform = glm::rotate(baseTransform, angleY, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec4 color1, color2;
    if (team == TEAM_1) { color1 = g_team1Color1; color2 = g_team1Color2; }
    else { color1 = g_team2Color1; color2 = g_team2Color2; }

    // --- (NOVO) Lógica de Animação Atualizada ---
    float runAngle = 0.0f;
    float kickAngle = 0.0f;
    float jumpOffset = 0.0f;     // Para o pulo da comemoração
    float armRaiseAngle = 0.0f;  // Para levantar os braços

    if (g_gameState == STATE_RUNNING_UP) { 
        runAngle = sin(g_animationTimer * 10.0f); 
    }
    else if (g_gameState == STATE_KICKING) {
        float kickProgress = std::min(1.0f, g_animationTimer / 0.3f);
        if(kickProgress < 0.66f) { kickAngle = glm::mix(0.0f, glm::radians(-90.0f), kickProgress / 0.66f); }
        else { kickAngle = glm::mix(glm::radians(-90.0f), glm::radians(30.0f), (kickProgress - 0.66f) / 0.34f); }
    }
    else if (g_gameState == STATE_CELEBRATING) {
        // Pulo: abs(sin(...)) cria um movimento de "pulo" contínuo
        jumpOffset = abs(sin(g_animationTimer * 8.0f)) * 0.4f; 
        // Braços para cima: Gira -135 graus no eixo X
        armRaiseAngle = glm::radians(-135.0f); 
    }
    // --- Fim da Lógica de Animação ---

    // Aplica o pulo (se estiver comemorando)
    baseTransform = glm::translate(baseTransform, glm::vec3(0.0f, jumpOffset, 0.0f));

    // --- Desenho do Corpo (semelhante ao anterior) ---
    glm::vec3 torsoSize = g_playerTorsoSize; glm::vec3 limbSize = g_playerLimbSize; float headRadius = g_playerHeadRadius;
    glm::vec3 neckSize(headRadius * 0.5f, 0.1f, headRadius * 0.5f);
    glm::vec3 handSize(limbSize.x * 0.8f, limbSize.x * 0.8f, limbSize.x * 0.8f);
    glm::vec3 footSize(limbSize.x * 1.1f, 0.15f, limbSize.z * 1.8f);
    int numStripes = 4; float stripeHeight = torsoSize.y / numStripes; glm::vec3 stripeSize = glm::vec3(torsoSize.x, stripeHeight, torsoSize.z);
    
    glBindVertexArray(cubeVAO);
    for (int i = 0; i < numStripes; ++i) {
        float yOffset = -torsoSize.y/2.0f + stripeHeight / 2.0f + i * stripeHeight;
        glm::mat4 torsoModel = glm::scale(glm::translate(baseTransform, glm::vec3(0.0f, yOffset, 0.0f)), stripeSize);
        glm::vec4 color = (i % 2 == 0) ? color1 : color2; drawCube(shaderProgram, torsoModel, color);
    }
    glm::mat4 neckModel_base = glm::translate(baseTransform, glm::vec3(0.0f, torsoSize.y / 2.0f, 0.0f));
    glm::mat4 neckModel_final = glm::scale(glm::translate(neckModel_base, glm::vec3(0.0f, neckSize.y / 2.0f, 0.0f)), neckSize);
    drawCube(shaderProgram, neckModel_final, g_skinColor);
    
    glm::mat4 headModel = glm::scale(glm::translate(neckModel_base, glm::vec3(0.0f, neckSize.y + headRadius * 0.8f, 0.0f)), glm::vec3(headRadius));
    drawSphere(shaderProgram, sphereVAO, sphereIndexCount, headModel, g_skinColor);
    
    glBindVertexArray(cubeVAO);
    glm::mat4 coxaEsqModel_base = glm::translate(baseTransform, glm::vec3(-0.15f, -torsoSize.y/2.0f, 0.0f));
    coxaEsqModel_base = glm::rotate(coxaEsqModel_base, glm::radians(30.0f) * -runAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 coxaEsqModel_final = glm::scale(glm::translate(coxaEsqModel_base, glm::vec3(0.0f, -limbSize.y/2.0f, 0.0f)), limbSize);
    drawCube(shaderProgram, coxaEsqModel_final, g_shortsColor);
    
    // ... (código da canela e pé esquerdo) ...
    glm::mat4 canelaEsqModel_base = glm::translate(coxaEsqModel_base, glm::vec3(0.0f, -limbSize.y, 0.0f));
    canelaEsqModel_base = glm::rotate(canelaEsqModel_base, glm::radians(20.0f) * std::max(0.0f, -runAngle), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 canelaEsqModel_final = glm::scale(glm::translate(canelaEsqModel_base, glm::vec3(0.0f, -limbSize.y/2.0f, 0.0f)), limbSize);
    drawCube(shaderProgram, canelaEsqModel_final, g_shortsColor);
    glm::mat4 peEsqModel_base = glm::translate(canelaEsqModel_base, glm::vec3(0.0f, -limbSize.y, 0.0f)); 
    glm::mat4 peEsqModel_final = glm::scale(glm::translate(peEsqModel_base, glm::vec3(0.0f, -footSize.y / 2.0f, footSize.z / 3.0f)), footSize);
    drawCube(shaderProgram, peEsqModel_final, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f)); 

    glm::mat4 coxaDirModel_base = glm::translate(baseTransform, glm::vec3(0.15f, -torsoSize.y/2.0f, 0.0f));
    float legAngle = (g_gameState == STATE_KICKING) ? kickAngle : (glm::radians(30.0f) * runAngle);
    coxaDirModel_base = glm::rotate(coxaDirModel_base, legAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 coxaDirModel_final = glm::scale(glm::translate(coxaDirModel_base, glm::vec3(0.0f, -limbSize.y/2.0f, 0.0f)), limbSize);
    drawCube(shaderProgram, coxaDirModel_final, g_shortsColor);

    // ... (código da canela e pé direito) ...
    glm::mat4 canelaDirModel_base = glm::translate(coxaDirModel_base, glm::vec3(0.0f, -limbSize.y, 0.0f));
    float kneeAngle = (g_gameState == STATE_KICKING) ? std::max(0.0f, -kickAngle * 0.5f) : (glm::radians(20.0f) * std::max(0.0f, runAngle));
    canelaDirModel_base = glm::rotate(canelaDirModel_base, kneeAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 canelaDirModel_final = glm::scale(glm::translate(canelaDirModel_base, glm::vec3(0.0f, -limbSize.y/2.0f, 0.0f)), limbSize);
    drawCube(shaderProgram, canelaDirModel_final, g_shortsColor);
    glm::mat4 peDirModel_base = glm::translate(canelaDirModel_base, glm::vec3(0.0f, -limbSize.y, 0.0f));
    glm::mat4 peDirModel_final = glm::scale(glm::translate(peDirModel_base, glm::vec3(0.0f, -footSize.y / 2.0f, footSize.z / 3.0f)), footSize);
    drawCube(shaderProgram, peDirModel_final, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));

    // --- (ATUALIZADO) Braços ---
    glm::mat4 bracoEsqModel_base = glm::translate(baseTransform, glm::vec3(-torsoSize.x/2.0f, torsoSize.y * 0.4f, 0.0f));
    bracoEsqModel_base = glm::rotate(bracoEsqModel_base, armRaiseAngle, glm::vec3(1.0f, 0.0f, 0.0f)); // <-- Aplicando rotação da comemoração
    bracoEsqModel_base = glm::rotate(bracoEsqModel_base, glm::radians(30.0f) * runAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 bracoEsqModel_final = glm::scale(glm::translate(bracoEsqModel_base, glm::vec3(-limbSize.x/2.0f, -limbSize.y/2.0f, 0.0f)), limbSize);
    drawCube(shaderProgram, bracoEsqModel_final, color1);
    
    // ... (mão esquerda) ...
    glm::mat4 maoEsqModel_base = glm::translate(bracoEsqModel_base, glm::vec3(-limbSize.x/2.0f, -limbSize.y, 0.0f));
    glm::mat4 maoEsqModel_final = glm::scale(glm::translate(maoEsqModel_base, glm::vec3(0.0f, -handSize.y/2.0f, 0.0f)), handSize);
    drawCube(shaderProgram, maoEsqModel_final, g_skinColor);

    glm::mat4 bracoDirModel_base = glm::translate(baseTransform, glm::vec3(torsoSize.x/2.0f, torsoSize.y * 0.4f, 0.0f));
    bracoDirModel_base = glm::rotate(bracoDirModel_base, armRaiseAngle, glm::vec3(1.0f, 0.0f, 0.0f)); // <-- Aplicando rotação da comemoração
    bracoDirModel_base = glm::rotate(bracoDirModel_base, glm::radians(30.0f) * -runAngle, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 bracoDirModel_final = glm::scale(glm::translate(bracoDirModel_base, glm::vec3(limbSize.x/2.0f, -limbSize.y/2.0f, 0.0f)), limbSize);
    drawCube(shaderProgram, bracoDirModel_final, color1);

    // ... (mão direita) ...
    glm::mat4 maoDirModel_base = glm::translate(bracoDirModel_base, glm::vec3(limbSize.x/2.0f, -limbSize.y, 0.0f));
    glm::mat4 maoDirModel_final = glm::scale(glm::translate(maoDirModel_base, glm::vec3(0.0f, -handSize.y/2.0f, 0.0f)), handSize);
    drawCube(shaderProgram, maoDirModel_final, g_skinColor);
}

void drawKeeper(unsigned int shaderProgram, unsigned int cubeVAO, unsigned int sphereVAO, int sphereIndexCount, glm::vec3 position, glm::vec4 color) {
    glm::mat4 baseTransform = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, g_keeperPosition.y, position.z));
    float diveRotationZ = 0.0f; float armRotationX = 0.0f; float armRotationY = 0.0f; float jumpY = 0.0f; bool stayMiddle = false;
    if (g_keeperState == KEEPER_DIVING) {
        float totalDist = abs(g_keeperTargetPos.x - g_keeperPosition.x); float diveProgress = 0.0f;
        if (totalDist > 0.01f) { diveProgress = 1.0f - (abs(position.x - g_keeperTargetPos.x) / totalDist); diveProgress = std::min(1.0f, std::max(0.0f, diveProgress)); }
        else { stayMiddle = true; float timeSinceDiveStart = g_animationTimer; diveProgress = std::min(1.0f, timeSinceDiveStart / 0.3f); }
        if (std::isnan(diveProgress) || std::isinf(diveProgress)) diveProgress = 0.0f;
        if (!stayMiddle) {
            jumpY = sin(diveProgress * PI) * 0.4f;
            diveRotationZ = glm::mix(0.0f, glm::radians(g_keeperTargetPos.x > g_keeperPosition.x ? -80.0f : 80.0f), diveProgress);
            armRotationY = glm::mix(0.0f, glm::radians(g_keeperTargetPos.x > g_keeperPosition.x ? -90.0f : 90.0f), diveProgress); // Levanta para os lados
        } else {
            armRotationX = glm::mix(0.0f, glm::radians(-90.0f), sin(diveProgress * PI)); // Estica para frente
        }
    }
    baseTransform = glm::translate(baseTransform, glm::vec3(0.0f, jumpY, 0.0f));
    baseTransform = glm::rotate(baseTransform, diveRotationZ, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 torsoSize = g_keeperTorsoSize; glm::vec3 limbSize = g_keeperLimbSize; float headRadius = g_keeperHeadRadius;
    glm::vec3 neckSize(headRadius * 0.5f, 0.1f, headRadius * 0.5f);
    glm::vec3 handSize(limbSize.x * 1.3f, limbSize.x * 1.3f, limbSize.x * 1.3f);
    glm::vec3 footSize(limbSize.x * 1.1f, 0.15f, limbSize.z * 1.8f);
    glBindVertexArray(cubeVAO);
    drawCube(shaderProgram, glm::scale(baseTransform, torsoSize), color);
    glm::mat4 neckModel_base = glm::translate(baseTransform, glm::vec3(0.0f, torsoSize.y / 2.0f, 0.0f));
    glm::mat4 neckModel_final = glm::scale(glm::translate(neckModel_base, glm::vec3(0.0f, neckSize.y / 2.0f, 0.0f)), neckSize);
    drawCube(shaderProgram, neckModel_final, g_skinColor);
    glm::mat4 headModel = glm::scale(glm::translate(neckModel_base, glm::vec3(0.0f, neckSize.y + headRadius * 0.8f, 0.0f)), glm::vec3(headRadius));
    drawSphere(shaderProgram, sphereVAO, sphereIndexCount, headModel, g_skinColor);
    glBindVertexArray(cubeVAO);
    glm::vec3 legSize(0.2f, 0.4f, 0.2f);
    glm::mat4 leftLegModel_base = glm::translate(baseTransform, glm::vec3(-0.15f, -torsoSize.y/2.0f - legSize.y/2.0f, 0.0f));
    glm::mat4 leftLegModel_final = glm::scale(leftLegModel_base, legSize);
    drawCube(shaderProgram, leftLegModel_final, g_team1Color1);
    glm::mat4 peEsqModel_base = glm::translate(leftLegModel_base, glm::vec3(0.0f, -legSize.y/2.0f - footSize.y / 2.0f, 0.0f));
    glm::mat4 peEsqModel_final = glm::scale(glm::translate(peEsqModel_base, glm::vec3(0.0f, 0.0f, footSize.z / 3.0f)), footSize);
    drawCube(shaderProgram, peEsqModel_final, g_team1Color1); // Chuteira Preta
    glm::mat4 rightLegModel_base = glm::translate(baseTransform, glm::vec3(0.15f, -torsoSize.y/2.0f - legSize.y/2.0f, 0.0f));
    glm::mat4 rightLegModel_final = glm::scale(rightLegModel_base, legSize);
    drawCube(shaderProgram, rightLegModel_final, g_team1Color1);
    glm::mat4 peDirModel_base = glm::translate(rightLegModel_base, glm::vec3(0.0f, -legSize.y/2.0f - footSize.y / 2.0f, 0.0f));
    glm::mat4 peDirModel_final = glm::scale(glm::translate(peDirModel_base, glm::vec3(0.0f, 0.0f, footSize.z / 3.0f)), footSize);
    drawCube(shaderProgram, peDirModel_final, g_team1Color1); // Chuteira Preta
    glm::vec3 shoulderL = glm::vec3(-torsoSize.x/2.0f, torsoSize.y * 0.4f, 0.0f);
    glm::vec3 shoulderR = glm::vec3( torsoSize.x/2.0f, torsoSize.y * 0.4f, 0.0f);
    glm::mat4 leftArmModel_base = glm::translate(baseTransform, shoulderL);
    leftArmModel_base = glm::rotate(leftArmModel_base, armRotationX, glm::vec3(1.0f, 0.0f, 0.0f));
    leftArmModel_base = glm::rotate(leftArmModel_base, armRotationY, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotação Y APLICADA
    glm::mat4 leftArmModel_final = glm::translate(leftArmModel_base, glm::vec3(-limbSize.x/2.0f, -limbSize.y / 2.0f, 0.0f));
    leftArmModel_final = glm::scale(leftArmModel_final, limbSize);
    drawCube(shaderProgram, leftArmModel_final, color);
    glm::mat4 maoEsqModel_base = glm::translate(leftArmModel_base, glm::vec3(-limbSize.x/2.0f, -limbSize.y, 0.0f));
    glm::mat4 maoEsqModel_final = glm::scale(glm::translate(maoEsqModel_base, glm::vec3(0.0f, -handSize.y/2.0f, 0.0f)), handSize);
    drawCube(shaderProgram, maoEsqModel_final, g_skinColor);
    glm::mat4 rightArmModel_base = glm::translate(baseTransform, shoulderR);
    rightArmModel_base = glm::rotate(rightArmModel_base, armRotationX, glm::vec3(1.0f, 0.0f, 0.0f));
    rightArmModel_base = glm::rotate(rightArmModel_base, armRotationY, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotação Y APLICADA
    glm::mat4 rightArmModel_final = glm::translate(rightArmModel_base, glm::vec3(limbSize.x/2.0f, -limbSize.y / 2.0f, 0.0f));
    rightArmModel_final = glm::scale(rightArmModel_final, limbSize);
    drawCube(shaderProgram, rightArmModel_final, color);
    glm::mat4 maoDirModel_base = glm::translate(rightArmModel_base, glm::vec3(limbSize.x/2.0f, -limbSize.y, 0.0f));
    glm::mat4 maoDirModel_final = glm::scale(glm::translate(maoDirModel_base, glm::vec3(0.0f, -handSize.y/2.0f, 0.0f)), handSize);
    drawCube(shaderProgram, maoDirModel_final, g_skinColor);
}

// --- Funções de Cenário ---
void drawField(unsigned int shaderProgram, unsigned int cubeVAO) {
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.1f, 0.0f)), glm::vec3(20.0f, 0.2f, 25.0f)), glm::vec4(0.0f, 0.5f, 0.1f, 1.0f));
}
void drawFieldMarkings(unsigned int shaderProgram, unsigned int cubeVAO) {
    glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    float lineY = 0.01f; float lineWidth = 0.1f;
    float fieldWidth = 20.0f; float fieldDepth = 25.0f; float halfWidth = fieldWidth / 2.0f; float halfDepth = fieldDepth / 2.0f;
    float farGoalLineZ = -10.0f; float nearGoalLineZ = halfDepth;
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, lineY, farGoalLineZ)), glm::vec3(fieldWidth, 0.01f, lineWidth)), white);
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, lineY, nearGoalLineZ)), glm::vec3(fieldWidth, 0.01f, lineWidth)), white);
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-halfWidth, lineY, 0.0f)), glm::vec3(lineWidth, 0.01f, fieldDepth)), white);
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(halfWidth, lineY, 0.0f)), glm::vec3(lineWidth, 0.01f, fieldDepth)), white);
    float penaltyAreaWidth = 8.0f; float penaltyAreaDepth = 4.0f; float penaltySpotZ = 6.0f; // Posição corrigida
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, lineY, farGoalLineZ + penaltyAreaDepth)), glm::vec3(penaltyAreaWidth, 0.01f, lineWidth)), white);
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-penaltyAreaWidth/2.0f, lineY, farGoalLineZ + penaltyAreaDepth/2.0f)), glm::vec3(lineWidth, 0.01f, penaltyAreaDepth)), white);
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(penaltyAreaWidth/2.0f, lineY, farGoalLineZ + penaltyAreaDepth/2.0f)), glm::vec3(lineWidth, 0.01f, penaltyAreaDepth)), white);
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0, lineY, penaltySpotZ)), glm::vec3(0.2f, 0.01f, 0.2f)), white);
}
void drawGoal(unsigned int shaderProgram, unsigned int cubeVAO, unsigned int cylinderVAO, int cylinderIndexCount) {
    glm::vec4 goalColor(0.9f, 0.9f, 0.9f, 1.0f);
    glm::vec4 netColor(0.9f, 0.9f, 0.9f, 0.4f);
    float goalLineZ = -10.0f;
    float postRadius = 0.08f;
    float netDepth = 1.5f;
    float backNetZ = goalLineZ - netDepth;
    float supportPostOffset = 0.1f;
    float supportPostsZ = backNetZ - supportPostOffset;
    float backPostHeight = g_goalHeight * 1.0f;

    // --- Animation Calculation ---
    float netBackZOffset = 0.0f;

    if (g_netAnimationTimer > 0.0f) {
        float bulgeAmount = sin((0.5f - g_netAnimationTimer) / 0.5f * PI);
        netBackZOffset = bulgeAmount * -0.5f;
    }
    float animatedGoalBackNetZ = backNetZ + netBackZOffset;

    // --- Draw Goal Frame (Cylinders) ---
    glm::mat4 leftPostModel = glm::translate(glm::mat4(1.0f), glm::vec3(-g_goalWidth/2.0f, g_goalHeight/2.0f, goalLineZ));
    drawCylinder(shaderProgram, cylinderVAO, cylinderIndexCount, leftPostModel, g_goalHeight, postRadius, goalColor);
    glm::mat4 rightPostModel = glm::translate(glm::mat4(1.0f), glm::vec3(g_goalWidth/2.0f, g_goalHeight/2.0f, goalLineZ));
    drawCylinder(shaderProgram, cylinderVAO, cylinderIndexCount, rightPostModel, g_goalHeight, postRadius, goalColor);
    glm::mat4 crossbarModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, g_goalHeight, goalLineZ));
    crossbarModel = glm::rotate(crossbarModel, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    drawCylinder(shaderProgram, cylinderVAO, cylinderIndexCount, crossbarModel, g_goalWidth, postRadius, goalColor);

    // --- Draw Back Support Structure (Cylinders) ---
    glm::mat4 backLeftPostModel = glm::translate(glm::mat4(1.0f), glm::vec3(-g_goalWidth/2.0f, backPostHeight/2.0f, supportPostsZ));
    drawCylinder(shaderProgram, cylinderVAO, cylinderIndexCount, backLeftPostModel, backPostHeight, postRadius * 0.8f, goalColor);
    glm::mat4 backRightPostModel = glm::translate(glm::mat4(1.0f), glm::vec3(g_goalWidth/2.0f, backPostHeight/2.0f, supportPostsZ));
    drawCylinder(shaderProgram, cylinderVAO, cylinderIndexCount, backRightPostModel, backPostHeight, postRadius * 0.8f, goalColor);


    // --- Barras Diagonais Superiores (CORRIGIDO) ---
    float diagonalRadius = postRadius * 0.7f;
    glm::vec3 yAxis(0.0f, 1.0f, 0.0f); // Default cylinder orientation

    // Barra Diagonal Esquerda
    glm::vec3 startLeft = glm::vec3(-g_goalWidth/2.0f, g_goalHeight, goalLineZ);
    glm::vec3 endLeft = glm::vec3(-g_goalWidth/2.0f, backPostHeight, supportPostsZ);
    glm::vec3 centerLeft = (startLeft + endLeft) / 2.0f;
    float lengthLeft = glm::distance(startLeft, endLeft);
    glm::vec3 directionLeft = glm::normalize(endLeft - startLeft);

    // Calculate rotation axis and angle
    glm::vec3 axisLeft = glm::cross(yAxis, directionLeft);
    float dotLeft = glm::dot(yAxis, directionLeft);
    // Clamp dot product to avoid acos domain error
    dotLeft = std::max(-1.0f, std::min(1.0f, dotLeft));
    float angleLeft = glm::acos(dotLeft);

    glm::mat4 rotationLeft = glm::rotate(glm::mat4(1.0f), angleLeft, axisLeft);
    glm::mat4 scaleLeft = glm::scale(glm::mat4(1.0f), glm::vec3(diagonalRadius, lengthLeft, diagonalRadius)); // Scale applied last
    glm::mat4 diagLeftModel = glm::translate(glm::mat4(1.0f), centerLeft) * rotationLeft * scaleLeft;
    // Pass the combined matrix to a simplified drawCylinder (or adjust drawCylinder)
    // Assuming drawCylinder takes the full model matrix including scale:
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(diagLeftModel));
    glUniform4f(glGetUniformLocation(shaderProgram, "objectColor"), goalColor.r, goalColor.g, goalColor.b, goalColor.a);
    glBindVertexArray(cylinderVAO);
    glDrawElements(GL_TRIANGLES, cylinderIndexCount, GL_UNSIGNED_INT, 0);


    // Barra Diagonal Direita
    glm::vec3 startRight = glm::vec3(g_goalWidth/2.0f, g_goalHeight, goalLineZ);
    glm::vec3 endRight = glm::vec3(g_goalWidth/2.0f, backPostHeight, supportPostsZ);
    glm::vec3 centerRight = (startRight + endRight) / 2.0f;
    float lengthRight = glm::distance(startRight, endRight);
    glm::vec3 directionRight = glm::normalize(endRight - startRight);

    glm::vec3 axisRight = glm::cross(yAxis, directionRight);
    float dotRight = glm::dot(yAxis, directionRight);
    dotRight = std::max(-1.0f, std::min(1.0f, dotRight));
    float angleRight = glm::acos(dotRight);

    glm::mat4 rotationRight = glm::rotate(glm::mat4(1.0f), angleRight, axisRight);
    glm::mat4 scaleRight = glm::scale(glm::mat4(1.0f), glm::vec3(diagonalRadius, lengthRight, diagonalRadius));
    glm::mat4 diagRightModel = glm::translate(glm::mat4(1.0f), centerRight) * rotationRight * scaleRight;

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(diagRightModel));
    // Color already set
    glDrawElements(GL_TRIANGLES, cylinderIndexCount, GL_UNSIGNED_INT, 0);
    // --- FIM Barras Diagonais ---


    // --- Draw Net (Transparent Thin Cubes) ---
    glBindVertexArray(cubeVAO);
    float netThickness = 0.02f;
    // Back Net
    drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, g_goalHeight/2.0f, animatedGoalBackNetZ)), glm::vec3(g_goalWidth, g_goalHeight, netThickness)), netColor);
    // Left Side Net
    glm::mat4 leftSideNetModel = glm::translate(glm::mat4(1.0f), glm::vec3(-g_goalWidth/2.0f, g_goalHeight/2.0f, goalLineZ - netDepth/2.0f));
    leftSideNetModel = glm::rotate(leftSideNetModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(shaderProgram, glm::scale(leftSideNetModel, glm::vec3(netDepth, g_goalHeight, netThickness)), netColor);
    // Right Side Net
    glm::mat4 rightSideNetModel = glm::translate(glm::mat4(1.0f), glm::vec3(g_goalWidth/2.0f, g_goalHeight/2.0f, goalLineZ - netDepth/2.0f));
    rightSideNetModel = glm::rotate(rightSideNetModel, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    drawCube(shaderProgram, glm::scale(rightSideNetModel, glm::vec3(netDepth, g_goalHeight, netThickness)), netColor);
    // Top Net
    glm::mat4 topNetModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, g_goalHeight, goalLineZ - netDepth/2.0f));
    topNetModel = glm::rotate(topNetModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    drawCube(shaderProgram, glm::scale(topNetModel, glm::vec3(g_goalWidth, netDepth, netThickness)), netColor);

    glBindVertexArray(0); // Unbind VAO after drawing everything for this goal
}

// void drawCrowd(unsigned int shaderProgram, unsigned int cubeVAO, unsigned int sphereVAO, int sphereIndexCount) {
    
//     // --- O "Truque" ---
//     // Salva o estado atual e força um estado ocioso (parado)
//     // para que a torcida não comece a correr ou chutar junto com o jogador.
//     GameState originalState = g_gameState;
//     g_gameState = STATE_READY;

//     // --- Parâmetros (baseados em drawGrandstands) ---
//     int numSteps = 4;           // N. de degraus
//     float stepWidth = 1.0f;
//     float stepHeight = 0.5f;
//     float fieldEdgeX = 10.0f;   // Metade da largura do campo
//     float fieldLength = 25.0f;  // Comprimento total do campo
    
//     float spacingZ = 2.0f; // Espaço entre cada torcedor no eixo Z
//     int spectatorsPerRow = (int)(fieldLength / spacingZ);

//     // Loop pelas fileiras (degraus)
//     for (int i = 0; i < numSteps; ++i) {
        
//         // Posição Y do "chão" do degrau onde o torcedor ficará
//         // (Centro do degrau + metade da altura do degrau)
//         float yPos = ((stepHeight / 2.0f) + (i * stepHeight)) + (stepHeight / 2.0f);
        
//         // Posição X do centro do degrau (Lado Esquerdo)
//         float xPosLeft = -fieldEdgeX - (stepWidth / 2.0f) - (i * stepWidth);
        
//         // Posição X do centro do degrau (Lado Direito)
//         float xPosRight = fieldEdgeX + (stepWidth / 2.0f) + (i * stepWidth);

//         // Loop pelos "assentos" (ao longo do eixo Z)
//         for (int j = 0; j < spectatorsPerRow; ++j) {
            
//             // Calcula a posição Z de cada torcedor
//             // Começa no fundo (-12.5) e avança
//             float zPos = -(fieldLength / 2.0f) + (spacingZ / 2.0f) + (j * spacingZ);
            
//             // --- Desenha Torcedor da Esquerda (Time 1 - Preto) ---
//             glm::vec3 posLeft(xPosLeft, yPos, zPos);
//             // Usamos TEAM_1 para a cor preta/branca
//             drawPlayer(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount, posLeft, TEAM_1);
            
//             // --- Desenha Torcedor da Direita (Time 2 - Azul) ---
//             glm::vec3 posRight(xPosRight, yPos, zPos);
//             // Usamos TEAM_2 para a cor azul
//             drawPlayer(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount, posRight, TEAM_2);
//         }
//     }
    
//     // Restaura o estado original do jogo
//     g_gameState = originalState;
// }

/**
 * (ATUALIZADO) Desenha a torcida, com lógica de comemoração
 */
void drawCrowd(unsigned int shaderProgram, unsigned int cubeVAO, unsigned int sphereVAO, int sphereIndexCount) {
    
    // Salva o estado ATUAL do jogo
    GameState realGameState = g_gameState;
    // Pega o time que chutou (se for gol, é o time que comemora)
    Team teamThatScored = g_currentKicker; 

    // --- Parâmetros (baseados em drawGrandstands) ---
    int numSteps = 4;           // N. de degraus
    float stepWidth = 1.0f;
    float stepHeight = 0.5f;
    float fieldEdgeX = 10.0f;   // Metade da largura do campo
    float fieldLength = 25.0f;  // Comprimento total do campo
    
    float spacingZ = 2.0f; // Espaço entre cada torcedor no eixo Z
    int spectatorsPerRow = (int)(fieldLength / spacingZ);

    // Loop pelas fileiras (degraus)
    for (int i = 0; i < numSteps; ++i) {
        
        float yPos = ((stepHeight / 2.0f) + (i * stepHeight)) + (stepHeight / 2.0f);
        float xPosLeft = -fieldEdgeX - (stepWidth / 2.0f) - (i * stepWidth);
        float xPosRight = fieldEdgeX + (stepWidth / 2.0f) + (i * stepWidth);

        // Loop pelos "assentos" (ao longo do eixo Z)
        for (int j = 0; j < spectatorsPerRow; ++j) {
            
            float zPos = -(fieldLength / 2.0f) + (spacingZ / 2.0f) + (j * spacingZ);
            
            // --- Desenha Torcedor da Esquerda (Time 1 - Preto) ---
            
            // Lógica de Animação:
            // Se o estado real é GOAL e o Time 1 marcou, comemore!
            if (realGameState == STATE_GOAL && teamThatScored == TEAM_1) {
                g_gameState = STATE_CELEBRATING; // Ativa a comemoração
            } else {
                g_gameState = STATE_READY; // Senão, fique parado
            }
            glm::vec3 posLeft(xPosLeft, yPos, zPos);
            drawPlayer(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount, posLeft, TEAM_1);
            
            // --- Desenha Torcedor da Direita (Time 2 - Azul) ---
            
            // Lógica de Animação:
            // Se o estado real é GOAL e o Time 2 marcou, comemore!
            if (realGameState == STATE_GOAL && teamThatScored == TEAM_2) {
                g_gameState = STATE_CELEBRATING; // Ativa a comemoração
            } else {
                g_gameState = STATE_READY; // Senão, fique parado
            }
            glm::vec3 posRight(xPosRight, yPos, zPos);
            drawPlayer(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount, posRight, TEAM_2);
        }
    }
    
    // Restaura o estado real do jogo (MUITO IMPORTANTE)
    g_gameState = realGameState;
}

void drawGrandstands(unsigned int shaderProgram, unsigned int cubeVAO) {
    glm::vec4 concreteColor(0.5f, 0.5f, 0.5f, 1.0f); // Cor de concreto
    
    int numSteps = 4;           // Número de "degraus" da arquibancada
    float stepWidth = 1.0f;     // Largura (em X) de cada degrau
    float stepHeight = 0.5f;    // Altura (em Y) de cada degrau
    
    // Dimensões do campo (para posicionamento)
    float fieldWidth = 24.0f;   // Total em X
    float fieldLength = 25.0f;  // Total em Z
    
    // Metade da largura do campo (onde a linha lateral está)
    float fieldEdgeX = fieldWidth / 2.0f; 
    // Fundo do campo (onde a linha de fundo está, atrás do gol)
    float fieldBackZ = -fieldLength / 2.0f; 

    // --- Arquibancadas Laterais (X negativo e positivo) ---
    // O comprimento (em Z) das arquibancadas laterais é o mesmo do campo
    float lateralStepLength = fieldLength;
    glm::vec3 lateralStepSize(stepWidth, stepHeight, lateralStepLength);

    // Arquibancada Esquerda (X negativo)
    for (int i = 0; i < numSteps; ++i) {
        float xPos = -fieldEdgeX - (stepWidth / 2.0f) - (i * stepWidth);
        float yPos = (stepHeight / 2.0f) + (i * stepHeight);
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(xPos, yPos, 0.0f));
        model = glm::scale(model, lateralStepSize);
        drawCube(shaderProgram, model, concreteColor);
    }

    // Arquibancada Direita (X positivo)
    for (int i = 0; i < numSteps; ++i) {
        float xPos = fieldEdgeX + (stepWidth / 2.0f) + (i * stepWidth);
        float yPos = (stepHeight / 2.0f) + (i * stepHeight);
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(xPos, yPos, 0.0f));
        model = glm::scale(model, lateralStepSize);
        drawCube(shaderProgram, model, concreteColor);
    }

    // --- (NOVO) Arquibancada Traseira (Z negativo, atrás do gol) ---
    // O comprimento (em X) da arquibancada traseira deve cobrir a largura do campo + as laterais da arquibancada
    float backStepLengthX = fieldWidth - 8 + (numSteps * stepWidth * 2.0f); // Largura do campo + larguras das arquibancadas laterais
    float backStepLengthZ = stepWidth; // A "largura" de cada degrau em Z

    glm::vec3 backStepSize(backStepLengthX, stepHeight, backStepLengthZ);

    for (int i = 0; i < numSteps; ++i) {
        float xPos = 0.0f; // Centralizada em X
        float yPos = (stepHeight / 2.0f) + (i * stepHeight);
        // Posição Z: atrás da linha de fundo do campo, e cada degrau mais para trás
        float zPos = fieldBackZ - (backStepLengthZ / 2.0f) - (i * backStepLengthZ); 
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(xPos, yPos, zPos));
        model = glm::scale(model, backStepSize);
        drawCube(shaderProgram, model, concreteColor);
    }
}

void drawScoreboard(unsigned int shaderProgram, unsigned int cubeVAO) {
    glm::vec3 scorePos(3.5f, 3.5f, -10.0f);
    float cubeSize = 0.4f; float spacing = 0.5f;
    glm::vec4 gray(0.3f, 0.3f, 0.3f, 1.0f);
    glm::vec4 green(0.1f, 0.8f, 0.1f, 1.0f);
    glm::vec4 red(0.8f, 0.1f, 0.1f, 1.0f);
    for (int i = 0; i < 3; ++i) {
        glm::vec4 color = gray; if (g_team1Results[i] == 1) color = green; if (g_team1Results[i] == 2) color = red;
        drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), scorePos + glm::vec3(i * spacing, 0.0f, 0.0f)), glm::vec3(cubeSize)), color);
    }
    for (int i = 0; i < 3; ++i) {
        glm::vec4 color = gray; if (g_team2Results[i] == 1) color = green; if (g_team2Results[i] == 2) color = red;
        drawCube(shaderProgram, glm::scale(glm::translate(glm::mat4(1.0f), scorePos + glm::vec3(i * spacing, -spacing, 0.0f)), glm::vec3(cubeSize)), color);
    }
}


// --- Colisão e Funções de Texto/Teclado ---
bool checkCollision(glm::vec3 pos1, glm::vec3 size1, glm::vec3 pos2, float radius2) {
    glm::vec3 half1 = size1 * 0.5f;
    glm::vec3 closest;
    closest.x = std::max(pos1.x - half1.x, std::min(pos2.x, pos1.x + half1.x));
    closest.y = std::max(pos1.y - half1.y, std::min(pos2.y, pos1.y + half1.y));
    // FIX: usar pos1.z (antes estava pos1.x erroneamente)
    closest.z = std::max(pos1.z - half1.z, std::min(pos2.z, pos1.z + half1.z));
    float distance = glm::length(closest - pos2);
    return distance < radius2;
}
void printKickMessage() {
    std::cout << "\n--- Vez do Time " << (g_currentKicker == TEAM_1 ? "1 (Listrado)" : "2 (Azul)") << " ---" << std::endl;
    std::cout << "Chute " << (g_currentKick / 2) + 1 << " de 3.\n" << std::endl;
    std::cout << "Escolha onde chutar:\n" << "1) Meio\n" << "2) Direita (Canto)\n" << "3) Esquerda (Canto)\n"
              << "--------------------------------" << std::endl;
}
void printFinalScore() {
    int score1 = 0; int score2 = 0;
    for(int r : g_team1Results) if(r == 1) score1++;
    for(int r : g_team2Results) if(r == 1) score2++;
    std::cout << "\n\n======= FIM DE JOGO! =======" << std::endl;
    std::cout << "  Placar Final: \n" << "  Time 1 (Listrado): " << score1 << "\n" << "  Time 2 (Azul):     " << score2 << std::endl;
    if (score1 > score2) std::cout << "  Time 1 VENCEU!" << std::endl;
    else if (score2 > score1) std::cout << "  Time 2 VENCEU!" << std::endl;
    else std::cout << "  EMPATE!" << std::endl;
    std::cout << "===========================" << std::endl;
}
void key_callback(GLFWwindow* window, int key, int scode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) { glfwSetWindowShouldClose(window, true); }
    if (g_gameState == STATE_READY && action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) g_kickRequest = 1;
        else if (key == GLFW_KEY_2) g_kickRequest = 2;
        else if (key == GLFW_KEY_3) g_kickRequest = 3;
    }
}


// --- PROGRAMA PRINCIPAL ---
int main() {
    // --- INICIALIZAÇÃO ---
    assert(glfwInit() == GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Disputa de Penaltis 3D (v5 Animado)", nullptr, nullptr);
    assert(window);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    assert(glewInit() == GLEW_OK);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- COMPILAÇÃO DOS SHADERS DE ILUMINAÇÃO ---
    unsigned int shaderProgram = glCreateProgram();
    {
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &lightingVertexShader, NULL); glCompileShader(vertexShader);
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &lightingFragmentShader, NULL); glCompileShader(fragmentShader);
        glAttachShader(shaderProgram, vertexShader); glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        glDeleteShader(vertexShader); glDeleteShader(fragmentShader);
    }
    
    // --- CRIAÇÃO DAS GEOMETRIAS ---
    unsigned int cubeVAO = createCubeVAO();
    std::pair<unsigned int, int> sphereData = createSphereVAO(1.0f, 32, 16);
    unsigned int sphereVAO = sphereData.first;
    int sphereIndexCount = sphereData.second;
    std::pair<unsigned int, int> cylinderData = createCylinderVAO(1.0f, 1.0f, 24);
    unsigned int cylinderVAO = cylinderData.first;
    int cylinderIndexCount = cylinderData.second;
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Chame isso uma vez antes do loop para definir a posição inicial
    updateCamera();
    
    printKickMessage();
    float lastFrameTime = 0.0f;

    // --- LOOP PRINCIPAL DE RENDERIZAÇÃO ---
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // --- LÓGICA DE ATUALIZAÇÃO ---
        glfwPollEvents();
        
        // ... (TODA A SUA LÓGICA DE JOGO VEM AQUI) ...
        // (if (g_netAnimationTimer > 0.0f)...)
        // (if (g_gameState != STATE_GAMEOVER)...)
        // (etc...)
        if (g_netAnimationTimer > 0.0f) { g_netAnimationTimer -= deltaTime; }
        if (g_gameState == STATE_RUNNING_UP || g_gameState == STATE_KICKING || g_keeperState == KEEPER_DIVING || g_gameState == STATE_GOAL) {
            g_animationTimer += deltaTime;
        }
        if (g_gameState != STATE_GAMEOVER) {
            if (g_gameState == STATE_READY && g_kickRequest != 0) {
                g_gameState = STATE_RUNNING_UP;
                g_animationTimer = 0.0f;
                std::cout << "Jogador correndo..." << std::endl;
            }
            if (g_gameState == STATE_RUNNING_UP) {
                glm::vec2 playerPosXZ(g_playerPosition.x, g_playerPosition.z);
                glm::vec2 ballPosXZ(g_ballPosition.x, g_ballPosition.z);
                glm::vec2 directionXZ = glm::normalize(ballPosXZ - playerPosXZ);
                g_playerPosition.x += directionXZ.x * g_playerRunSpeed * deltaTime;
                g_playerPosition.z += directionXZ.y * g_playerRunSpeed * deltaTime;
                if (glm::distance(playerPosXZ, ballPosXZ) < 0.5f) {
                    g_playerPosition.x = g_ballPosition.x - directionXZ.x * 0.5f;
                    g_playerPosition.z = g_ballPosition.z - directionXZ.y * 0.5f;
                    g_gameState = STATE_KICKING;
                    g_animationTimer = 0.0f;
                    std::cout << "CHUTOU!" << std::endl;
                    float targetX = 0.0f;
                    if (g_kickRequest == 2) targetX = (g_goalWidth / 2.0f) * 0.8f;
                    if (g_kickRequest == 3) targetX = -(g_goalWidth / 2.0f) * 0.8f;
                    glm::vec3 kickDirection = glm::vec3(targetX, 0.5f, g_keeperPosition.z) - g_ballPosition;
                    g_ballVelocity = glm::normalize(kickDirection) * g_ballSpeed;
                    int choice = g_keeperChoice(g_randomEngine);
                    float keeperTargetX = 0.0f;
                    if (choice == 1) keeperTargetX = -(g_goalWidth / 2.0f) * 0.8f;
                    if (choice == 3) keeperTargetX = (g_goalWidth / 2.0f) * 0.8f;
                    g_keeperTargetPos = glm::vec3(keeperTargetX, g_keeperPosition.y, g_keeperPosition.z);
                    g_keeperState = KEEPER_DIVING;
                    g_animationTimer = 0.0f; 
                    g_kickRequest = 0;
                }
            }
            if (g_gameState == STATE_KICKING) {
                if (g_animationTimer > 0.3f) {
                    g_gameState = STATE_BALL_IN_FLIGHT; 
                }
            }
            if (g_gameState == STATE_BALL_IN_FLIGHT) {
                // Atualiza posição da bola
                g_ballPosition += g_ballVelocity * deltaTime;
                int currentKickIndex = g_currentKick / 2;

                // 1) Colisão com goleiro (prioridade)
                if (checkCollision(g_keeperPosition, g_keeperTorsoSize, g_ballPosition, g_ballRadius) && !g_goalRecorded) {
                    // Defesa do goleiro: rebate para frente (em direção ao jogador)
                    g_gameState = STATE_SAVED;
                    // Mantém componente X mas reduz, e inverte Z para ir para frente do campo
                    g_ballVelocity = glm::vec3(g_ballVelocity.x * 0.3f, std::max(0.1f, g_ballVelocity.y * 0.2f), 2.5f);
                    // Aumenta um pouco o tempo de rebote para a animação ficar visível
                    g_reboundTimer = 0.8f;
                    std::cout << "DEFENDEU!!!" << std::endl;
                    if (g_currentKicker == TEAM_1) g_team1Results[currentKickIndex] = 2; else g_team2Results[currentKickIndex] = 2;
                    g_currentKick++;
                }
                else {
                    // 2) Verifica se cruzou a linha do gol (entrada no arco)
                    if (!g_goalRecorded && g_ballPosition.z < g_goalLineZ) {
                        bool insideWidth = std::abs(g_ballPosition.x) <= (g_goalWidth / 2.0f);
                        bool underCrossbar = g_ballPosition.y <= g_goalHeight;
                        if (insideWidth && underCrossbar) {
                            // Marca gol (a bola segue até a rede traseira)
                            g_goalRecorded = true;
                            g_gameState = STATE_GOAL; // <-- MUDA O ESTADO
                            // define tempo para a animação da rede e para manter o estado antes do reset
                            g_netAnimationTimer = 0.5f;
                            g_resetTimer = 2.5f; // <-- importante: dá tempo para bola chegar na rede e animação
                            if (g_currentKicker == TEAM_1) g_team1Results[currentKickIndex] = 1; else g_team2Results[currentKickIndex] = 1;
                            std::cout << "GOOOOOOL! Bola entrou no gol (registrado)." << std::endl;
                            g_currentKick++;
                            // Deixa a velocidade original para que a bola percorra até a rede traseira
                        } else {
                            // Passou a linha mas não dentro do arco => bola perdida (fora)
                            g_gameState = STATE_RESETTING;
                            g_resetTimer = 2.0f;
                            std::cout << "Fora do gol." << std::endl;
                            g_currentKick++;
                        }
                    }
                    
                }
            }
            if (g_keeperState == KEEPER_DIVING) {
                g_keeperPosition.x = glm::mix(g_keeperPosition.x, g_keeperTargetPos.x, g_keeperDiveSpeed * deltaTime);
                if (std::abs(g_keeperPosition.x - g_keeperTargetPos.x) < 0.1f) {
                    g_keeperPosition.x = g_keeperTargetPos.x;
                }
            }
            if (g_gameState == STATE_SAVED) {
                g_reboundTimer -= deltaTime;
                g_ballPosition += g_ballVelocity * deltaTime;
                if (g_reboundTimer <= 0.0f) {
                    g_gameState = STATE_RESETTING; g_resetTimer = 2.0f; g_ballVelocity = glm::vec3(0.0f);
                }
            }
            // 6. ESTADO DE GOL
            if (g_gameState == STATE_GOAL) {
                // (NOVO!) ATUALIZA A FÍSICA DA BOLA PARA ELA CONTINUAR ATÉ A REDE
                g_ballPosition += g_ballVelocity * deltaTime;

                // (MOVIDO PARA CÁ!) VERIFICA COLISÃO COM A REDE TRASEIRA
                // (A flag g_goalRecorded já é verdadeira se estamos neste estado)
                if (g_ballPosition.z < (g_backNetZ + 0.05f)) {
                    // Trava a bola na rede
                    g_ballPosition.z = g_backNetZ + 0.05f;
                    
                    // Rebate (inverte Z e reduz velocidade)
                    g_ballVelocity.z = 1.0f;
                    g_ballVelocity.x *= 0.05f;
                    g_ballVelocity.y = 0.1f; // Pequeno "pop" para cima

                    // Reduz o tempo de reset, pois a bola já parou
                    g_resetTimer = 2.0f;
                }

                // (NOVO) VERIFICA SE A BOLA ESTÁ "SAINDO" DO GOL APÓS REBATER
                // Só ativa se a bola estiver vindo para frente (vel Z > 0)
                if (g_ballVelocity.z > 0 && g_ballPosition.z > (g_goalLineZ - 0.05f)) {
                    // Trava a bola na linha do gol
                    g_ballPosition.z = g_goalLineZ - 0.05f;
                    // Para a bola completamente
                    g_ballVelocity = glm::vec3(0.0f);
                }

                // (Opcional) Mini-gravidade para a bola "cair" no chão após bater
                if (g_ballPosition.y > g_ballRadius + 0.01f) {
                    g_ballVelocity.y -= 2.0f * deltaTime; 
                } else {
                    g_ballPosition.y = g_ballRadius;
                    g_ballVelocity.y = 0.0f;
                }

                // Apenas decrementar timers;
                if (g_netAnimationTimer > 0.0f) { g_netAnimationTimer -= deltaTime; }
                if (g_resetTimer > 0.0f) { g_resetTimer -= deltaTime; }
                
                if (g_resetTimer <= 0.0f) {
                    g_gameState = STATE_RESETTING;
                    g_netAnimationTimer = 0.0f;
                }
            }
            if (g_gameState == STATE_RESETTING) {
                if(g_resetTimer > 0.0f) { g_resetTimer -= deltaTime; }
                if (g_resetTimer <= 0.0f) {
                    if (g_currentKick == 6) {
                        g_gameState = STATE_GAMEOVER; printFinalScore(); glfwSetWindowShouldClose(window, true); 
                    } else {
                        g_gameState = STATE_READY; g_keeperState = KEEPER_IDLE; g_animationTimer = 0.0f;
                        g_currentKicker = (g_currentKicker == TEAM_1) ? TEAM_2 : TEAM_1; 
                        g_playerPosition = g_playerStartPos;
                        g_ballPosition = glm::vec3(0.0f, 0.1f, 6.0f); 
                        g_keeperPosition = glm::vec3(0.0f, 0.8f, -10.0f); 
                        g_goalRecorded = false; 
                        printKickMessage();
                    }
                }
            }
        } // Fim do if(STATE_GAMEOVER)

        // --- LÓGICA DE DESENHO (RENDER) ---
        glClearColor(0.1f, 0.2f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);

        // --- Bloco ÚNICO e CORRETO de Câmera ---
        // 1. Processa os inputs de teclado
        processInput(window);

        // 2. Atualiza a posição da câmera e a view matrix
        updateCamera(); 

        // 3. Envia as matrizes e posições atualizadas para o Shader
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(g_viewMatrix)); // Usa a g_viewMatrix
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(g_lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(g_cameraPos)); // Usa a g_cameraPos
        // --- Fim do Bloco de Câmera ---

        // --- Desenha Objetos OPACOS ---
        glBindVertexArray(cubeVAO);
        drawField(shaderProgram, cubeVAO);
        drawFieldMarkings(shaderProgram, cubeVAO);
        drawScoreboard(shaderProgram, cubeVAO);

        // glBindVertexArray(cubeVAO);
        // drawField(shaderProgram, cubeVAO);
        // drawFieldMarkings(shaderProgram, cubeVAO);
        // drawScoreboard(shaderProgram, cubeVAO);
        
        drawGrandstands(shaderProgram, cubeVAO); // <-- ADICIONE AQUI

        drawCrowd(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount);
        
        // Goleiro (Amarelo, com animação de mergulho)
        drawKeeper(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount, g_keeperPosition, g_keeperColor);
        
        // Desenha o jogador ATIVO (com animação de corrida/chute)
        if (g_gameState != STATE_GAMEOVER) {
            drawPlayer(shaderProgram, cubeVAO, sphereVAO, sphereIndexCount, g_playerPosition, g_currentKicker);
        }

        // Bola (Esfera)
        drawSphere(shaderProgram, sphereVAO, sphereIndexCount, glm::scale(glm::translate(glm::mat4(1.0f), g_ballPosition), glm::vec3(g_ballRadius)), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // --- Desenha Objetos TRANSPARENTES (Rede) por último ---
        // Usa cylinderVAO para traves
        drawGoal(shaderProgram, cubeVAO, cylinderVAO, cylinderIndexCount);

        glfwSwapBuffers(window);
    }
    // --- LIMPEZA ---
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteVertexArrays(1, &cylinderVAO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}