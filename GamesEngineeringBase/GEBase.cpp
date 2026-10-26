#include <iostream>
#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <string>
#include <stdexcept>
#include <random>

// -----------------------------------------------------------

#include "Entity.h" // Include the Entity header
#include "GameEntity.h" // Include the GameEntity header
//#include "Raid.h" // Include the Raid header

// -----------------------------------------------------------

#include <filesystem>
#include <windows.h>
#include <algorithm>

// -----------------------------------------------------------

int main() {
    
	// Debug: Print the current working directory and check if the resource file exists
    wchar_t cwd[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, cwd);
    std::wcerr << L"CWD = " << cwd << L"\n";
    std::cerr << "exists = " << std::filesystem::exists("..\\Resources\\hero_knight.png") << "\n";

    // Create a timer object to measure time intervals.
    GamesEngineeringBase::Timer timer; 
    
    // Create a canvas window with dimensions 1024x768 and title “Tiles"
    GamesEngineeringBase::Window canvas;
    canvas.create(1024, 768, "Tiles");
    
    BackgroundEntity background({ 0, 0 });
    SpriteEntity sprite({ 100, 100 }, "..\\Resources\\hero_knight.png");
    HeroEntity hero({ 100.0f, static_cast<float>(canvas.getHeight()) - 128.0f });

	float delta_time = timer.dt(); // Get the elapsed time since the last frame.

    bool running = true; // Variable to control the main loop's running state.

    while (running)
    {
		
        delta_time = timer.dt(); // Update the elapsed time for the current frame.
        
        // Check for input (key presses or window events)
        if (canvas.keyPressed(VK_ESCAPE)) break;

        // Clear the window for the next frame rendering
        canvas.clear(95, 200, 255);
        
        // Update game logic

        Vec2f input{ 0, 0 };
        if (canvas.keyPressed('W')) input.y_ = -2.0f;
        if (canvas.keyPressed('S')) input.y_ = 2.0f;
        if (canvas.keyPressed('A')) input.x_ = -2.0f;
        if (canvas.keyPressed('D')) input.x_ = 2.0f;

        // Draw();         
		background.Update(delta_time);
		sprite.Update(delta_time);

		hero.SetInputAxis(input);
		hero.Update(delta_time);

		// Keep the hero inside the canvas
		const float maxX = static_cast<float>(canvas.getWidth()) - static_cast<float>(hero.GetImageWidth());
		const float maxY = static_cast<float>(canvas.getHeight()) - static_cast<float>(hero.GetImageHeight());
		hero.SetPosition({ std::clamp(hero.GetPosition().x_, 0.0f, maxX),
						   std::clamp(hero.GetPosition().y_, 0.0f, maxY) });

		background.Draw(canvas);
		sprite.Draw(canvas);
		hero.Draw(canvas);

        // Draw a vertical white line through the center of the canvas
        for (int y = 0; y < canvas.getHeight(); y++)
        {
            canvas.draw(512, y, 255, 255, 255);
        }

        // Display the frame on the screen. This must be called once the frame is finished in order to display the frame.
        canvas.present();
    }
    return 0;
}