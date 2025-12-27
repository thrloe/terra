#include "raylib.h"
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <string>

const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;
const int DEFAULT_GRID_SIZE = 10;
const int IMPULSE_COST = 3;
const float AI_DELAY = 1.0f;
const int WIN_PERCENTAGE = 45;

enum class CellState { NEUTRAL, PLAYER, AI };
enum class GameState { MENU, PLAYING, GAME_OVER };
enum class GameResult { NONE, PLAYER_WIN, AI_WIN, DRAW };
enum class ImpulseMode { NONE, ATTACK, SPEED };

GameState currentState = GameState::MENU;
GameResult gameResult = GameResult::NONE;
int gridSize = DEFAULT_GRID_SIZE;
int impulseCost = IMPULSE_COST;
bool playerTurn = true;
bool impulseModeActive = false;
ImpulseMode currentImpulseMode = ImpulseMode::NONE;
bool musicEnabled = true;

// меню
int selectedGridSize = 1; // 0 = 7x7, 1 = 10x10, 2 = 12x12
bool gridSizeButtons[3] = {false, true, false};
bool musicButtonActive = true;

// поле
std::vector<std::vector<CellState>> grid;
int playerCells = 1;
int aiCells = 1;
int playerCharges = 0;
int aiCharges = 0;

// AI
float aiTimer = 0.0f;
std::mt19937 rng(std::random_device{}());

// UI
float cellSize;
float gridOffsetX;
float gridOffsetY;
std::pair<int, int> selectedCell = {-1, -1};
std::vector<std::pair<int, int>> selectedCells;

Sound captureSound;
Sound impulseSound;
Sound attackSound;
Sound winSound;
Sound loseSound;
Sound drawSound;
Music backgroundMusic;

void initializeGame();
void resetGame();
void updateGame(float deltaTime);
void renderGame();
void handleInput();
void processPlayerTurn();
void processAITurn(float deltaTime);
void checkWinCondition();
std::vector<std::pair<int, int>> getAvailableMoves(CellState player);
bool isValidMove(int x, int y, CellState player);
std::vector<std::pair<int, int>> getAdjacentCells(int x, int y);
void renderCell(int x, int y, CellState state, bool isSelected = false);
void startImpulseMode(ImpulseMode mode);
void handleImpulseCellSelection(int x, int y);
void finishImpulseMode();
std::pair<int, int> getAIMove();
std::vector<std::pair<int, int>> getAIAttackMove();
std::vector<std::pair<int, int>> getAISpeedMove();
ImpulseMode decideAIImpulseMode();
void loadSounds();
void playCaptureSound();
void playImpulseSound();
void playAttackSound();
void playWinSound();
void playLoseSound();
void playDrawSound();
void toggleMusic();
void renderMenuButtons();
void updateMenu();

int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Territorial Control");
    SetTargetFPS(60);
    
    InitAudioDevice();
    loadSounds();
    
    if (musicEnabled) {
        PlayMusicStream(backgroundMusic);
    }
    
    initializeGame();
    
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        
        UpdateMusicStream(backgroundMusic);
        
        handleInput();
        updateGame(deltaTime);
        
        BeginDrawing();
        ClearBackground(BLACK);
        renderGame();
        EndDrawing();
    }
    
    UnloadSound(captureSound);
    UnloadSound(impulseSound);
    UnloadSound(attackSound);
    UnloadSound(winSound);
    UnloadSound(loseSound);
    UnloadSound(drawSound);
    UnloadMusicStream(backgroundMusic);
    CloseAudioDevice();
    
    CloseWindow();
    return 0;
}

void loadSounds() {
    captureSound = LoadSound("assets/sounds/Key.mp3");
    if (captureSound.stream.buffer == NULL) {
        TraceLog(LOG_WARNING, "Failed to load capture sound");
    }
    
    impulseSound = LoadSound("assets/sounds/Door.mp3");
    if (impulseSound.stream.buffer == NULL) {
        TraceLog(LOG_WARNING, "Failed to load impulse sound");
    }
    
    attackSound = LoadSound("assets/sounds/Clark.mp3");
    if (attackSound.stream.buffer == NULL) {
        TraceLog(LOG_WARNING, "Failed to load attack sound");
    }
    
    winSound = LoadSound("assets/sounds/Key.mp3");
    loseSound = LoadSound("assets/sounds/Door.mp3");
    drawSound = LoadSound("assets/sounds/Clark.mp3");
    
    // Загружаем фоновую музыку (играет постоянно в фоне)
    backgroundMusic = LoadMusicStream("assets/sounds/Key.mp3");
    if (backgroundMusic.stream.buffer == NULL) {
        TraceLog(LOG_WARNING, "Failed to load background music");
    } else {
        backgroundMusic.looping = true;
    }
}

void playCaptureSound() {
    if (captureSound.stream.buffer != NULL) {
        PlaySound(captureSound);
    }
}

void playImpulseSound() {
    if (impulseSound.stream.buffer != NULL) {
        PlaySound(impulseSound);
    }
}

void playAttackSound() {
    if (attackSound.stream.buffer != NULL) {
        PlaySound(attackSound);
    }
}

void playWinSound() {
    if (winSound.stream.buffer != NULL) {
        PlaySound(winSound);
    }
}

void playLoseSound() {
    if (loseSound.stream.buffer != NULL) {
        PlaySound(loseSound);
    }
}

void playDrawSound() {
    if (drawSound.stream.buffer != NULL) {
        PlaySound(drawSound);
    }
}

void toggleMusic() {
    musicEnabled = !musicEnabled;
    if (musicEnabled) {
        ResumeMusicStream(backgroundMusic);
    } else {
        PauseMusicStream(backgroundMusic);
    }
}

void initializeGame() {
    // Создаем поле
    grid.resize(gridSize, std::vector<CellState>(gridSize, CellState::NEUTRAL));
    
    // Начальные позиции
    grid[0][0] = CellState::PLAYER;
    grid[gridSize-1][gridSize-1] = CellState::AI;
    
    playerCells = 1;
    aiCells = 1;
    playerCharges = 0;
    aiCharges = 0;
    playerTurn = true;
    impulseModeActive = false;
    currentImpulseMode = ImpulseMode::NONE;
    
    // Рассчитываем размеры для отрисовки
    float maxGridSize = std::min(WINDOW_WIDTH * 0.8f, WINDOW_HEIGHT * 0.8f);
    cellSize = maxGridSize / gridSize;
    gridOffsetX = (WINDOW_WIDTH - cellSize * gridSize) / 2;
    gridOffsetY = (WINDOW_HEIGHT - cellSize * gridSize) / 2 + 50;
}

void resetGame() {
    // Очищаем поле
    for (int x = 0; x < gridSize; x++) {
        for (int y = 0; y < gridSize; y++) {
            grid[x][y] = CellState::NEUTRAL;
        }
    }
    
    // Восстанавливаем начальные позиции
    grid[0][0] = CellState::PLAYER;
    grid[gridSize-1][gridSize-1] = CellState::AI;
    
    playerCells = 1;
    aiCells = 1;
    playerCharges = 0;
    aiCharges = 0;
    playerTurn = true;
    impulseModeActive = false;
    currentImpulseMode = ImpulseMode::NONE;
    selectedCell = {-1, -1};
    selectedCells.clear();
    gameResult = GameResult::NONE;
    currentState = GameState::PLAYING;
}

void updateGame(float deltaTime) {
    switch (currentState) {
        case GameState::MENU:
            updateMenu();
            break;
            
        case GameState::PLAYING:
            if (gameResult != GameResult::NONE) {
                currentState = GameState::GAME_OVER;
                
                switch (gameResult) {
                    case GameResult::PLAYER_WIN:
                        playWinSound();
                        break;
                    case GameResult::AI_WIN:
                        playLoseSound();
                        break;
                    case GameResult::DRAW:
                        playDrawSound();
                        break;
                    default:
                        break;
                }
                break;
            }
            
            if (playerTurn) {
                processPlayerTurn();
            } else {
                processAITurn(deltaTime);
            }
            break;
            
        case GameState::GAME_OVER:
            if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = GameState::MENU;
            }
            break;
    }
}

void updateMenu() {
    Vector2 mousePos = GetMousePosition();
    
    // 7x7
    if (CheckCollisionPointRec(mousePos, Rectangle{200, 200, 100, 40})) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedGridSize = 0;
            gridSizeButtons[0] = true;
            gridSizeButtons[1] = false;
            gridSizeButtons[2] = false;
        }
    }
    
    // 10x10
    if (CheckCollisionPointRec(mousePos, Rectangle{320, 200, 100, 40})) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedGridSize = 1;
            gridSizeButtons[0] = false;
            gridSizeButtons[1] = true;
            gridSizeButtons[2] = false;
        }
    }
    
    // 12x12
    if (CheckCollisionPointRec(mousePos, Rectangle{440, 200, 100, 40})) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedGridSize = 2;
            gridSizeButtons[0] = false;
            gridSizeButtons[1] = false;
            gridSizeButtons[2] = true;
        }
    }
    
    // музыка
    if (CheckCollisionPointRec(mousePos, Rectangle{200, 260, 200, 40})) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            toggleMusic();
            musicButtonActive = musicEnabled;
        }
    }
    
    // начать игру
    if (CheckCollisionPointRec(mousePos, Rectangle{WINDOW_WIDTH/2 - 100, 350, 200, 50})) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            switch(selectedGridSize) {
                case 0: gridSize = 7; break;
                case 1: gridSize = 10; break;
                case 2: gridSize = 12; break;
            }
            
            resetGame();
            currentState = GameState::PLAYING;
        }
    }
    
    // выход
    if (CheckCollisionPointRec(mousePos, Rectangle{WINDOW_WIDTH/2 - 100, 420, 200, 50})) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            CloseWindow();
        }
    }
    
    // Горячие клавиши
    if (IsKeyPressed(KEY_SPACE)) {
        switch(selectedGridSize) {
            case 0: gridSize = 7; break;
            case 1: gridSize = 10; break;
            case 2: gridSize = 12; break;
        }
        
        resetGame();
        currentState = GameState::PLAYING;
    }
    
    if (IsKeyPressed(KEY_ESCAPE)) {
        CloseWindow();
    }
    
    if (IsKeyPressed(KEY_M)) {
        toggleMusic();
        musicButtonActive = musicEnabled;
    }
}

void renderGame() {
    switch (currentState) {
        case GameState::MENU:
            renderMenuButtons();
            break;
            
        case GameState::PLAYING:
            {
                // Отрисовка сетки
                for (int x = 0; x < gridSize; x++) {
                    for (int y = 0; y < gridSize; y++) {
                        bool isSelected = false;
                        if (impulseModeActive) {
                            for (auto& cell : selectedCells) {
                                if (x == cell.first && y == cell.second) {
                                    isSelected = true;
                                    break;
                                }
                            }
                        }
                        renderCell(x, y, grid[x][y], isSelected);
                    }
                }
                
                // UI информация
                int targetCells = (gridSize * gridSize * WIN_PERCENTAGE + 99) / 100;
                std::string targetStr = "Target: " + std::to_string(targetCells) + " cells (" + std::to_string(WIN_PERCENTAGE) + "%)";
                DrawText(targetStr.c_str(), WINDOW_WIDTH/2 - MeasureText(targetStr.c_str(), 20)/2, gridOffsetY - 40, 20, WHITE);
                
                std::string playerStr = "Player: " + std::to_string(playerCells) + "/" + std::to_string(targetCells);
                DrawText(playerStr.c_str(), WINDOW_WIDTH/2 - MeasureText(playerStr.c_str(), 20)/2, gridOffsetY + gridSize * cellSize + 10, 20, BLUE);
                
                std::string aiStr = "AI: " + std::to_string(aiCells) + "/" + std::to_string(targetCells);
                DrawText(aiStr.c_str(), WINDOW_WIDTH/2 - MeasureText(aiStr.c_str(), 20)/2, gridOffsetY - 30, 20, RED);
                
                std::string playerChargesStr = "Charges: " + std::to_string(playerCharges);
                DrawText(playerChargesStr.c_str(), WINDOW_WIDTH - 200, 50, 20, BLUE);
                
                std::string aiChargesStr = "AI Charges: " + std::to_string(aiCharges);
                DrawText(aiChargesStr.c_str(), 50, 50, 20, RED);
                
                // Индикатор хода
                std::string turnStr = playerTurn ? "YOUR TURN" : "AI TURN";
                Color turnColor = playerTurn ? BLUE : RED;
                DrawText(turnStr.c_str(), WINDOW_WIDTH/2 - MeasureText(turnStr.c_str(), 30)/2, 20, 30, turnColor);
                
                // Индикатор импульса
                if (impulseModeActive) {
                    std::string modeStr = currentImpulseMode == ImpulseMode::ATTACK ? 
                                        "ATTACK MODE: Select 2 enemy cells (left click)" : 
                                        "SPEED MODE: Select 3 neutral cells (left click)";
                    DrawText(modeStr.c_str(), WINDOW_WIDTH/2 - MeasureText(modeStr.c_str(), 20)/2, WINDOW_HEIGHT - 40, 20, YELLOW);
                    
                    // Счетчик выбранных клеток
                    std::string countStr = "Selected: " + std::to_string(selectedCells.size()) + "/" + 
                                         (currentImpulseMode == ImpulseMode::ATTACK ? "2" : "3");
                    DrawText(countStr.c_str(), WINDOW_WIDTH/2 - MeasureText(countStr.c_str(), 20)/2, WINDOW_HEIGHT - 65, 20, WHITE);
                }
                
                // Индикатор музыки (только статус, без реакции на действия)
                std::string musicStatus = musicEnabled ? "MUSIC: ON" : "MUSIC: OFF";
                Color musicColor = musicEnabled ? GREEN : GRAY;
                DrawText(musicStatus.c_str(), WINDOW_WIDTH - 150, WINDOW_HEIGHT - 20, 15, musicColor);
            }
            break;
            
        case GameState::GAME_OVER:
            {
                DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Fade(BLACK, 0.7f));
                
                const char* resultText = "";
                Color resultColor = WHITE;
                
                switch (gameResult) {
                    case GameResult::PLAYER_WIN:
                        resultText = "VICTORY!";
                        resultColor = GREEN;
                        break;
                    case GameResult::AI_WIN:
                        resultText = "DEFEAT!";
                        resultColor = RED;
                        break;
                    case GameResult::DRAW:
                        resultText = "DRAW!";
                        resultColor = YELLOW;
                        break;
                    default:
                        resultText = "GAME OVER";
                        resultColor = WHITE;
                        break;
                }
                
                DrawText(resultText, WINDOW_WIDTH/2 - MeasureText(resultText, 80)/2, WINDOW_HEIGHT/2 - 100, 80, resultColor);
                
                std::string finalScore = "Final Score: Player " + std::to_string(playerCells) + " - AI " + std::to_string(aiCells);
                DrawText(finalScore.c_str(), WINDOW_WIDTH/2 - MeasureText(finalScore.c_str(), 30)/2, WINDOW_HEIGHT/2, 30, WHITE);
                
                DrawText("Press R or ESC to return to menu", WINDOW_WIDTH/2 - MeasureText("Press R or ESC to return to menu", 20)/2, WINDOW_HEIGHT/2 + 80, 20, YELLOW);
                
                std::string musicStatus = musicEnabled ? "Music: ON (M to toggle)" : "Music: OFF (M to toggle)";
                Color musicColor = musicEnabled ? GREEN : GRAY;
                DrawText(musicStatus.c_str(), WINDOW_WIDTH/2 - MeasureText(musicStatus.c_str(), 15)/2, WINDOW_HEIGHT - 30, 15, musicColor);
            }
            break;
    }
}

void renderMenuButtons() {
    // Заголовок
    DrawText("TERRITORIAL CONTROL", WINDOW_WIDTH/2 - MeasureText("TERRITORIAL CONTROL", 40)/2, 50, 40, WHITE);
    
    // Выбор размера сетки
    DrawText("Grid Size:", 150, 150, 25, WHITE);
    
    // Кнопка 7x7
    Color button7x7Color = gridSizeButtons[0] ? YELLOW : LIGHTGRAY;
    DrawRectangle(200, 200, 100, 40, button7x7Color);
    DrawText("7x7", 230, 210, 20, BLACK);
    
    // Кнопка 10x10
    Color button10x10Color = gridSizeButtons[1] ? YELLOW : LIGHTGRAY;
    DrawRectangle(320, 200, 100, 40, button10x10Color);
    DrawText("10x10", 340, 210, 20, BLACK);
    
    // Кнопка 12x12
    Color button12x12Color = gridSizeButtons[2] ? YELLOW : LIGHTGRAY;
    DrawRectangle(440, 200, 100, 40, button12x12Color);
    DrawText("12x12", 460, 210, 20, BLACK);
    
    // Кнопка музыки
    Color musicButtonColor = musicButtonActive ? GREEN : GRAY;
    DrawRectangle(200, 260, 200, 40, musicButtonColor);
    DrawText(musicEnabled ? "MUSIC: ON" : "MUSIC: OFF", 240, 270, 20, BLACK);
    
    // Кнопка начала игры
    DrawRectangle(WINDOW_WIDTH/2 - 100, 350, 200, 50, YELLOW);
    DrawText("START GAME", WINDOW_WIDTH/2 - MeasureText("START GAME", 20)/2, 365, 20, BLACK);
    
    // Кнопка выхода
    DrawRectangle(WINDOW_WIDTH/2 - 100, 420, 200, 50, GRAY);
    DrawText("EXIT", WINDOW_WIDTH/2 - MeasureText("EXIT", 20)/2, 435, 20, BLACK);
    
    // Инструкции
    DrawText("Click buttons or use arrow keys + Enter", 
             WINDOW_WIDTH/2 - MeasureText("Click buttons or use arrow keys + Enter", 15)/2, 
             500, 15, LIGHTGRAY);
    DrawText("Press M to toggle music anytime", 
             WINDOW_WIDTH/2 - MeasureText("Press M to toggle music anytime", 15)/2, 
             530, 15, LIGHTGRAY);
    
    // Правила игры
    DrawText("Game Rules:", WINDOW_WIDTH/2 - 100, 570, 25, WHITE);
    DrawText("- Capture neutral cells adjacent to your territory", 100, 610, 15, LIGHTGRAY);
    DrawText("- Collect charge for special moves (3 charges needed)", 100, 630, 15, LIGHTGRAY);
    DrawText("- Press A for ATTACK impulse (remove 2 enemy cells)", 100, 650, 15, LIGHTGRAY);
    DrawText("- Press S for SPEED impulse (capture 3 neutral cells)", 100, 670, 15, LIGHTGRAY);
    
    std::string winText = "- First to " + std::to_string(WIN_PERCENTAGE) + "% of the board wins!";
    DrawText(winText.c_str(), 100, 690, 15, GREEN);
}

void handleInput() {
    switch (currentState) {
        case GameState::MENU:
            break;
            
        case GameState::PLAYING:
            if (IsKeyPressed(KEY_M)) {
                toggleMusic();
                musicButtonActive = musicEnabled;
            }
            
            if (playerTurn && !impulseModeActive) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    
                    int gridX = (mousePos.x - gridOffsetX) / cellSize;
                    int gridY = (mousePos.y - gridOffsetY) / cellSize;
                    
                    if (gridX >= 0 && gridX < gridSize && gridY >= 0 && gridY < gridSize) {
                        if (grid[gridX][gridY] == CellState::NEUTRAL && isValidMove(gridX, gridY, CellState::PLAYER)) {
                            grid[gridX][gridY] = CellState::PLAYER;
                            playerCells++;
                            playerCharges++;
                            playCaptureSound();
                            
                            playerTurn = false;
                            aiTimer = 0.0f;
                        }
                    }
                }
                
                if (playerCharges >= impulseCost) {
                    if (IsKeyPressed(KEY_A)) {
                        startImpulseMode(ImpulseMode::ATTACK);
                        playAttackSound();
                    } else if (IsKeyPressed(KEY_S)) {
                        startImpulseMode(ImpulseMode::SPEED);
                        playImpulseSound();
                    }
                }
            }
            
            if (impulseModeActive && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mousePos = GetMousePosition();
                int gridX = (mousePos.x - gridOffsetX) / cellSize;
                int gridY = (mousePos.y - gridOffsetY) / cellSize;
                
                if (gridX >= 0 && gridX < gridSize && gridY >= 0 && gridY < gridSize) {
                    handleImpulseCellSelection(gridX, gridY);
                }
            }
            break;
            
        case GameState::GAME_OVER:
            if (IsKeyPressed(KEY_M)) {
                toggleMusic();
                musicButtonActive = musicEnabled;
            }
            break;
    }
}

void processPlayerTurn() {
}

void processAITurn(float deltaTime) {
    aiTimer += deltaTime;
    
    if (aiTimer >= AI_DELAY) {
        if (aiCharges >= impulseCost && decideAIImpulseMode() == ImpulseMode::ATTACK) {
            auto attackCells = getAIAttackMove();
            for (auto& cell : attackCells) {
                if (cell.first != -1) {
                    grid[cell.first][cell.second] = CellState::AI;
                    playerCells--;
                    aiCells++;
                }
            }
            if (!attackCells.empty()) {
                aiCharges -= impulseCost;
                PlaySound(attackSound);
            }
        } else if (aiCharges >= impulseCost && decideAIImpulseMode() == ImpulseMode::SPEED) {
            auto cells = getAISpeedMove();
            for (auto& cell : cells) {
                if (cell.first != -1) {
                    grid[cell.first][cell.second] = CellState::AI;
                    aiCells++;
                }
            }
            if (!cells.empty()) {
                aiCharges -= impulseCost;
                PlaySound(impulseSound);
            }
        } else {
            auto move = getAIMove();
            if (move.first != -1) {
                grid[move.first][move.second] = CellState::AI;
                aiCells++;
                aiCharges++;
                PlaySound(captureSound);
            }
        }
        
        playerTurn = true;
        checkWinCondition();
    }
}

void checkWinCondition() {
    int totalCells = gridSize * gridSize;
    int targetCells = (totalCells * WIN_PERCENTAGE + 99) / 100;
    
    bool playerWin = (playerCells >= targetCells);
    bool aiWin = (aiCells >= targetCells);
    
    if (playerWin && aiWin) {
        gameResult = GameResult::DRAW;
    } else if (playerWin) {
        gameResult = GameResult::PLAYER_WIN;
    } else if (aiWin) {
        gameResult = GameResult::AI_WIN;
    }
}

void startImpulseMode(ImpulseMode mode) {
    if (playerCharges >= impulseCost) {
        impulseModeActive = true;
        currentImpulseMode = mode;
        selectedCells.clear();
    }
}

void handleImpulseCellSelection(int x, int y) {
    if (currentImpulseMode == ImpulseMode::ATTACK) {
        if (grid[x][y] == CellState::AI) {
            auto adjacent = getAdjacentCells(x, y);
            for (auto& adj : adjacent) {
                if (grid[adj.first][adj.second] == CellState::PLAYER) {
                    bool alreadySelected = false;
                    for (auto& cell : selectedCells) {
                        if (cell.first == x && cell.second == y) {
                            alreadySelected = true;
                            break;
                        }
                    }
                    
                    if (!alreadySelected && selectedCells.size() < 2) {
                        selectedCells.push_back({x, y});
                    }
                    
                    if (selectedCells.size() >= 2) {
                        for (auto& cell : selectedCells) {
                            grid[cell.first][cell.second] = CellState::PLAYER;
                            aiCells--;
                            playerCells++;
                        }
                        playerCharges -= impulseCost;
                        finishImpulseMode();
                        playerTurn = false;
                        aiTimer = 0.0f;
                    }
                    return;
                }
            }
        }
    } else if (currentImpulseMode == ImpulseMode::SPEED) {
        if (grid[x][y] == CellState::NEUTRAL && isValidMove(x, y, CellState::PLAYER)) {
            // Проверяем, не выбрана ли уже эта клетка
            bool alreadySelected = false;
            for (auto& cell : selectedCells) {
                if (cell.first == x && cell.second == y) {
                    alreadySelected = true;
                    break;
                }
            }
            
            if (!alreadySelected && selectedCells.size() < 3) {
                selectedCells.push_back({x, y});
            }
            
            // Если выбраны 3 клетки, применяем эффект
            if (selectedCells.size() >= 3) {
                for (auto& cell : selectedCells) {
                    grid[cell.first][cell.second] = CellState::PLAYER;
                    playerCells++;
                }
                playerCharges -= impulseCost;
                finishImpulseMode();
                playerTurn = false;
                aiTimer = 0.0f;
            }
        }
    }
}

void finishImpulseMode() {
    impulseModeActive = false;
    currentImpulseMode = ImpulseMode::NONE;
    selectedCells.clear();
}

std::vector<std::pair<int, int>> getAvailableMoves(CellState player) {
    std::vector<std::pair<int, int>> moves;
    
    for (int x = 0; x < gridSize; x++) {
        for (int y = 0; y < gridSize; y++) {
            if (grid[x][y] == CellState::NEUTRAL) {
                auto adjacent = getAdjacentCells(x, y);
                for (auto& adj : adjacent) {
                    if (grid[adj.first][adj.second] == player) {
                        moves.push_back({x, y});
                        break;
                    }
                }
            }
        }
    }
    
    return moves;
}

bool isValidMove(int x, int y, CellState player) {
    if (x < 0 || x >= gridSize || y < 0 || y >= gridSize || grid[x][y] != CellState::NEUTRAL) {
        return false;
    }
    
    auto adjacent = getAdjacentCells(x, y);
    for (auto& adj : adjacent) {
        if (grid[adj.first][adj.second] == player) {
            return true;
        }
    }
    
    return false;
}

std::vector<std::pair<int, int>> getAdjacentCells(int x, int y) {
    std::vector<std::pair<int, int>> adjacent;
    
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    for (int i = 0; i < 4; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        
        if (nx >= 0 && nx < gridSize && ny >= 0 && ny < gridSize) {
            adjacent.push_back({nx, ny});
        }
    }
    
    return adjacent;
}

void renderCell(int x, int y, CellState state, bool isSelected) {
    Color cellColor;
    
    switch (state) {
        case CellState::NEUTRAL: cellColor = GRAY; break;
        case CellState::PLAYER: cellColor = isSelected ? ColorAlpha(BLUE, 0.3f) : BLUE; break;
        case CellState::AI: cellColor = isSelected ? ColorAlpha(RED, 0.3f) : RED; break;
    }
    
    if (impulseModeActive && !isSelected) {
        cellColor = ColorAlpha(cellColor, 0.5f);
    }
    
    DrawRectangle(gridOffsetX + x * cellSize, gridOffsetY + y * cellSize,
                 cellSize - 1, cellSize - 1, cellColor);
    
    DrawRectangleLines(gridOffsetX + x * cellSize, gridOffsetY + y * cellSize,
                      cellSize, cellSize, WHITE);
    
    if (playerTurn && !impulseModeActive && state == CellState::NEUTRAL && isValidMove(x, y, CellState::PLAYER)) {
        DrawRectangleLines(gridOffsetX + x * cellSize + 2, gridOffsetY + y * cellSize + 2,
                          cellSize - 4, cellSize - 4, YELLOW);
    }
}

std::pair<int, int> getAIMove() {
    auto availableMoves = getAvailableMoves(CellState::AI);
    
    if (availableMoves.empty()) {
        return {-1, -1};
    }
    
    std::uniform_int_distribution<> dis(0, availableMoves.size() - 1);
    return availableMoves[dis(rng)];
}

std::vector<std::pair<int, int>> getAIAttackMove() {
    std::vector<std::pair<int, int>> attackableCells;
    
    for (int x = 0; x < gridSize; x++) {
        for (int y = 0; y < gridSize; y++) {
            if (grid[x][y] == CellState::PLAYER) {
                auto adjacent = getAdjacentCells(x, y);
                for (auto& adj : adjacent) {
                    if (grid[adj.first][adj.second] == CellState::AI) {
                        attackableCells.push_back({x, y});
                        break;
                    }
                }
            }
        }
    }
    
    std::vector<std::pair<int, int>> result;
    if (attackableCells.empty()) {
        return result;
    }
    
    std::shuffle(attackableCells.begin(), attackableCells.end(), rng);
    
    if (attackableCells.size() >= 2) {
        result.push_back(attackableCells[0]);
        result.push_back(attackableCells[1]);
    } else if (!attackableCells.empty()) {
        result.push_back(attackableCells[0]);
    }
    
    return result;
}

std::vector<std::pair<int, int>> getAISpeedMove() {
    auto availableMoves = getAvailableMoves(CellState::AI);
    
    std::vector<std::pair<int, int>> result;
    
    if (availableMoves.empty()) {
        return result;
    }
    
    std::vector<std::pair<int, int>> moves = availableMoves;
    std::shuffle(moves.begin(), moves.end(), rng);
    
    if (moves.size() >= 3) {
        result.push_back(moves[0]);
        result.push_back(moves[1]);
        result.push_back(moves[2]);
    } else if (moves.size() >= 2) {
        result.push_back(moves[0]);
        result.push_back(moves[1]);
    } else if (!moves.empty()) {
        result.push_back(moves[0]);
    }
    
    return result;
}

ImpulseMode decideAIImpulseMode() {
    std::uniform_real_distribution<> dis(0.0, 1.0);
    return (dis(rng) < 0.5) ? ImpulseMode::ATTACK : ImpulseMode::SPEED;
}