#include "SplitEngine/Window.hpp"
#include "SplitEngine/ErrorHandler.hpp"

#include <format>

namespace SplitEngine
{
	void Window::Open()
	{
		const int SCREEN_WIDTH  = 2000;
		const int SCREEN_HEIGHT = 1000;

		// Create _window
		_window = SDL_CreateWindow("Project Thalia", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

		if (_window == nullptr) { ErrorHandler::ThrowRuntimeError(std::format("Window could not be created! SDL_Error: {0}\n", SDL_GetError())); }
	}

	void Window::Close()
	{
		SDL_DestroyWindow(_window);
		SDL_Quit();
	}

	SDL_Window* Window::GetSDLWindow() const { return _window; }

	glm::ivec2 Window::GetSize() const
	{
		glm::ivec2 size;

		SDL_GetWindowSize(_window, &size.x, &size.y);

		return size;
	}

	bool Window::IsMinimized() const { return _isMinimized; }

	void Window::SetMinimized(bool newState) { _isMinimized = newState; }
}
