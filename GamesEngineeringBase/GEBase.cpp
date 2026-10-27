#include <iostream>
#include "GamesEngineeringBase.h" // Include the GamesEngineeringBase header
#include <string>
#include <stdexcept>
#include <random>

// -----------------------------------------------------------

#include "Entity.h" // Include the Entity header
#include "GameEntity.h" // Include the GameEntity header
#include "Raid.h" // Include the Raid header

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
	WorldEntity world({ 64.0f, 64.0f });

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

        // Hold SPACE to show the alpha masks and the water/land debug line
        world.SetShowAlphas(canvas.keyPressed(VK_SPACE));

        // Draw();         
		//background.Update(delta_time); // Comment this out after testing the scrolling tiles
		sprite.Update(delta_time);

		hero.SetInputAxis(input);

		const Vec2f previous_hero_position = hero.GetPosition();
		hero.Update(delta_time);
		world.Update(delta_time);

		// Water/land test: the hero's feet must stay on land, otherwise undo the move
		const Vec2f hero_feet{ hero.GetPosition().x_ + hero.GetImageWidth() / 2.0f,
							   hero.GetPosition().y_ + hero.GetImageHeight() - 1.0f };
		if (!world.IsLand(canvas, hero_feet))
		{
			hero.SetPosition(previous_hero_position);
		}

		// Keep the hero inside the canvas
		const float maxX = static_cast<float>(canvas.getWidth()) - static_cast<float>(hero.GetImageWidth());
		const float maxY = static_cast<float>(canvas.getHeight()) - static_cast<float>(hero.GetImageHeight());
		hero.SetPosition({ std::clamp(hero.GetPosition().x_, 0.0f, maxX),
						   std::clamp(hero.GetPosition().y_, 0.0f, maxY) });

		//background.Draw(canvas); // Comment this out after testing the scrolling tiles
        world.Draw(canvas);
        sprite.Draw(canvas);
		hero.Draw(canvas);


        // Display the frame on the screen. This must be called once the frame is finished in order to display the frame.
        canvas.present();
    }
    return 0;
}