#include <iostream>
#include <stdexcept>
#include "scaler.hpp"

using namespace oneam;
void require(bool value, const char* message)
{
   if (!value)
      throw std::runtime_error(message);
}
int main()
{
   try
   {
      Image checker{128, 128, std::vector<std::uint8_t>(128 * 128 * 4, 255)};
      for (int y = 0; y < 128; ++y)
         for (int x = 0; x < 128; ++x)
            for (int c = 0; c < 3; ++c)
               checker.pixels[(y * 128 + x) * 4 + c] = (x + y) % 2 ? 255 : 0;
      const auto original = checker.pixels;
      auto small = resize_image(checker, 8, 8);
      require(bool(small), "downsampling succeeds");
      for (int y = 1; y < 7; ++y)
         for (int x = 1; x < 7; ++x)
         {
            const auto i = (y * 8 + x) * 4;
            require(
               small->pixels[i] >= 186 && small->pixels[i] <= 190, "shrinking integrates source detail in linear light without aliasing"
            );
            require(small->pixels[i + 3] == 255, "opaque pages remain opaque");
         }
      require(checker.pixels == original, "source pixels remain available for zooming");
      Image solid{17, 31, std::vector<std::uint8_t>(17 * 31 * 4, 77)};
      auto reduced = resize_image(solid, 3, 5);
      require(bool(reduced), "solid reduction succeeds");
      for (auto pixel : reduced->pixels)
         require(pixel >= 76 && pixel <= 78, "constant colors preserved");
      require(!resize_image(checker, 0, 5) && !resize_image(checker, 16384, 16384), "output bounds enforced");
      require(!resize_image(Image{2, 2, {}}, 1, 1), "truncated input rejected");
      PageScaler worker;
      auto source = std::make_shared<Image>(std::move(checker));
      for (int i = 0; i < 100; ++i)
         worker.request(source, 3 + i % 10, 3 + i % 10);
      auto final = std::make_shared<Image>(std::move(solid));
      worker.request(final, 5, 9);
      bool received = false;
      const auto start = SDL_GetTicks();
      while (SDL_GetTicks() - start < 5000)
      {
         if (auto update = worker.poll())
         {
            require(
               update->source == final && update->image.width == 5 && update->image.height == 9,
               "only the newest page and size are published"
            );
            received = true;
            break;
         }
         SDL_Delay(2);
      }
      require(received, "filter worker completes");
      worker.request(source, 8, 8);
      worker.clear();
      SDL_Delay(30);
      require(!worker.poll(), "clearing cancels pending and completed results");
      std::cout << "Antialiased filtering and asynchronous cancellation passed.\n";
      return 0;
   }
   catch (const std::exception& error)
   {
      std::cerr << error.what() << '\n';
      return 1;
   }
}
