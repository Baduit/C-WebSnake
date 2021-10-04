#include <cheerp/clientlib.h>
#include <cheerp/client.h>
#include <chrono>
#include <functional>
#include <vector>
#include <list>
#include <random>
#include <algorithm>

constexpr int GAME_SCALE = 10;

// A class with 3 uint8_t or a bitfield with cool constructor would be better but for a project that small I'm a bit lazy
namespace color
{
constexpr int RED = 0x0000FF;
constexpr int BLACK = 0x000000;
constexpr int WHITE = 0xFFFFFF;
constexpr int GREEN = 0x00FF00;
} // namespace color

class [[cheerp::genericjs]] Graphics
{
	public:
		Graphics(int w = 400, int h = 300):
			_width(w),
			_height(h)
		{
			
		}

		void start()
		{
			_canvas = static_cast<client::HTMLCanvasElement*>(client::document.getElementById("snake_canvas"));
			_canvas->set_width(_width);
			_canvas->set_height(_height);
			_canvas_ctx = static_cast<client::CanvasRenderingContext2D*>(_canvas->getContext("2d"));

			client::requestAnimationFrame(cheerp::Callback([&](){ handle_refresh(); }));
			client::document.addEventListener("keydown", cheerp::Callback([&](client::KeyboardEvent* e){ key_down_handler(e); }));

			client::HTMLButtonElement* button = static_cast<client::HTMLButtonElement*>(client::document.getElementById("replay_button"));
			button->addEventListener("click", cheerp::Callback([&](){ replay_button(); }));

			_score_printer = static_cast<client::HTMLParagraphElement *>(client::document.getElementById("score_printer"));
		}

		void set_score(int score)
		{
			std::string text = "The score is: " + std::to_string(score);
			_score_printer->set_innerText(text.c_str());
		}

		void replay_button()
		{
			client::console.log("Replay clicked");
			if (_on_replay_cb)
				_on_replay_cb();
		}

		void key_down_handler(client::KeyboardEvent* e)
		{
			if (_key_event_cb)
				_key_event_cb(e);
		}
		
		void draw_rect(int x, int y, int w, int h, int rgb)
		{
			int r = rgb&0xff;
			int g = (rgb>>8)&0xff;
			int b = (rgb>>16)&0xff;
			_canvas_ctx->set_fillStyle(client::String("").concat("rgb(", r, ",", g, ",", b, ")"));
			_canvas_ctx->fillRect(x, y, w, h);
		}
		void debug_output(const char* str)
		{
			_canvas_ctx->set_font("24px sans-serif");
			_canvas_ctx->set_fillStyle("rgb(255,255,255)");
			_canvas_ctx->fillText(str, 0, _height - 24);
		}

		void handle_refresh()
		{
			if (_refresh_cb)
				_refresh_cb();
			client::requestAnimationFrame(cheerp::Callback([&](){ handle_refresh(); }));
		}

		template <typename Cb>
		void set_key_event_cb(Cb cb)
		{
			_key_event_cb = std::move(cb);
		}

		template <typename Cb>
		void set_refresh_cb(Cb cb)
		{
			_refresh_cb = std::move(cb);
		}

		template <typename Cb>
		void set_on_replay_cb(Cb cb)
		{
			_on_replay_cb = std::move(cb);
		}

	private:
		client::HTMLCanvasElement* _canvas = nullptr;
		client::CanvasRenderingContext2D* _canvas_ctx = nullptr;
		client::HTMLParagraphElement* _score_printer = nullptr;
		int _width = 400;
		int _height = 300;
		std::function<void()> _refresh_cb = nullptr;
		std::function<void(client::KeyboardEvent*)> _key_event_cb = nullptr;
		std::function<void()> _on_replay_cb = nullptr;
};

struct Pos
{
	int x;
	int y;
};
bool operator==(const Pos& a, const Pos& b) { return a.x == b.x && a.y == b.y; }
bool operator!=(const Pos& a, const Pos& b) { return !(a == b); }

struct Fruit
{
	Pos pos;
};

enum class Direction
{
	TOP,
	RIGHT,
	BOTTOM,
	LEFT
};

struct Snake
{
	Snake(Pos p)
	{
		direction = Direction::RIGHT;
		pos.push_back(p);
	}

	Pos get_next_head_pos()
	{
		if (direction == Direction::TOP)
		{
			return Pos{ pos.front().x, pos.front().y - 1};
		}
		else if (direction == Direction::RIGHT)
		{
			return Pos{ pos.front().x + 1, pos.front().y};
		}
		else if (direction == Direction::BOTTOM)
		{
			return Pos{ pos.front().x, pos.front().y + 1};
		}
		else// if (direction == Direction::LEFT)
		{
			return Pos{ pos.front().x - 1, pos.front().y};
		}
	}

	void move(bool is_eating)
	{
		pos.push_front(get_next_head_pos());
		if (!is_eating)
		{
			pos.pop_back();
		}
	}

	Direction direction;
	std::list<Pos> pos; // Head is front
};

using Duration = decltype(std::chrono::system_clock::now() - std::chrono::system_clock::now());
class SimpleClock
{
	public:
		SimpleClock() { reset(); };

		SimpleClock(const SimpleClock&) = default;
		SimpleClock& operator=(const SimpleClock&) = default;

		SimpleClock(SimpleClock&&) = default;
		SimpleClock& operator=(SimpleClock&&) = default;

		virtual ~SimpleClock() = default;

		void		reset() { _first = std::chrono::system_clock::now(); }
		
		Duration	get_time_duration() { return std::chrono::system_clock::now() - _first; }

		template<typename T>
		auto		get_duration_as() { return std::chrono::duration_cast<T>(get_time_duration()); }

		int64_t		get_time_nano_count() { return get_duration_as<std::chrono::nanoseconds>().count(); }

	private:
		std::chrono::system_clock::time_point	_first;
};

struct RandomEngine
{
	RandomEngine():
		_mt(_rd())
	{}

	std::size_t operator()(std::size_t max_value_included)
	{
		std::uniform_int_distribution<std::size_t> dist(0, max_value_included);
		return dist(_mt);
	}

	std::random_device	_rd;
	std::mt19937		_mt;
};

class Game
{
	private:
		enum class Collision
		{
			YES,
			NO
		};

	public:
		Game(int w = 40, int h = 30):
			_width(w),
			_height(h),
			_snake({10, 10})
		{
			add_fruit();
		}

		void reset()
		{
			_snake = Snake({10, 10});
			add_fruit();
			_clock.reset();
			_is_running = true;
		}

		void update()
		{
			if (!_is_running)
				return;

			using namespace std::chrono_literals;
			if (_clock.get_time_duration() < 150ms)
				return;

			bool is_snake_eating = will_snake_eat(_snake.get_next_head_pos());
			_snake.move(is_snake_eating);

			if (is_snake_eating)
				add_fruit();

			if (check_collision() == Collision::YES)
				_is_running = false;

			_clock.reset();
		}

		operator bool() const
		{
			return _is_running;
		}

		const Fruit& get_fruit() const
		{
			return _fruit;
		}

		const Snake& get_snake() const
		{
			return _snake;
		}

		int get_width() const
		{
			return _width;
		}

		int get_height() const
		{
			return _height;
		}

		void update_snake_direction(Direction d)
		{
			if (_snake.direction != d)
				client::console.log("Direction changed");
			_snake.direction = d;
		}

	private:
		Collision check_collision() const
		{
			if (_snake.pos.front().x < 0 || _snake.pos.front().x >= _width ||
				_snake.pos.front().y < 0 || _snake.pos.front().y >= _height)
			{
				return Collision::YES;
			}
			
			auto it = std::find(++(_snake.pos.begin()), _snake.pos.end(), _snake.pos.front());
			if (it != _snake.pos.end())
				return Collision::YES;

			return Collision::NO;
		}

		bool will_snake_eat(Pos next_head_pos) const
		{
			return next_head_pos == _fruit.pos;
		}

		bool is_fruit_on_snake() const
		{
			return _snake.pos.front() == _fruit.pos;
		}

		void add_fruit()
		{
			do
			{
				_fruit.pos.x = _rand(_width - 1);
				_fruit.pos.y = _rand(_height - 1);
			}
			while (is_fruit_on_snake());
		}

	private:
		int _width;
		int _height;
		Snake _snake;
		Fruit _fruit;
		bool _is_running = true;
		SimpleClock _clock;
		RandomEngine _rand;
};

struct Renderer
{
	Renderer(Graphics& g):
		graphics(g)
	{}

	void operator()(const Game& game)
	{
		graphics.draw_rect(0, 0, game.get_width() * GAME_SCALE, game.get_height() * GAME_SCALE, color::BLACK);
		
		for (auto snake_part: game.get_snake().pos)
		{
			client::console.log("SnakePart");
			graphics.draw_rect(snake_part.x * GAME_SCALE, snake_part.y * GAME_SCALE, GAME_SCALE, GAME_SCALE, color::GREEN);	
		}

		const auto& fruit = game.get_fruit();
		graphics.draw_rect(fruit.pos.x * GAME_SCALE, fruit.pos.y * GAME_SCALE, GAME_SCALE, GAME_SCALE, color::RED);

		int actual_score = game.get_snake().pos.size() - 1; 
		if (old_score != actual_score)
		{
			old_score = actual_score;
			graphics.set_score(actual_score);
		}


		if (!game)
			graphics.debug_output("You lost !");
	}

	int old_score = 0;
	Graphics& graphics;
};

Game game;
Graphics graphics;
Renderer renderer(graphics);

void webMain()
{
    client::console.log("Hello, World Wide Web!");

	graphics.set_refresh_cb(
		[&]()
		{
			game.update();
			renderer(game);
		});

	graphics.set_key_event_cb(
		[&](client::KeyboardEvent* e)
		{
			if(e->get_keyCode() == 37)
				game.update_snake_direction(Direction::LEFT);
			else if(e->get_keyCode() == 38)
				game.update_snake_direction(Direction::TOP);
			else if(e->get_keyCode() == 39)
				game.update_snake_direction(Direction::RIGHT);
			else if(e->get_keyCode() == 40)
				game.update_snake_direction(Direction::BOTTOM);
		});

	graphics.set_on_replay_cb(
		[&]()
		{
			game.reset();
		});
	
	graphics.start();
}