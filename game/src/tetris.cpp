#include "tetris.hpp"
#include "glad.h"
#include "pieces.hpp"
#include "state.hpp"
#include "types.hpp"
#include <random>
#include <set>

tetris::game::game(std::random_device& seed, eng::engine* e,
                   const std::filesystem::path& path)
    : random_engine{ seed() }
    , engine{ e } {

    e->load_texture((path / "textures" / "square.png").string(), 256, 256, 4,
                    GL_LUMINANCE);
    sounds["rotate"].reset(
        e->create_sound_buffer((path / "sounds" / "whoosh.wav").string()));
    sounds["start"].reset(
        e->create_sound_buffer((path / "sounds" / "start.wav").string()));
    sounds["move"].reset(
        e->create_sound_buffer((path / "sounds" / "move.wav").string()));
    sounds["clear"].reset(
        e->create_sound_buffer((path / "sounds" / "clear_row.wav").string()));
    sounds["automove"].reset(
        e->create_sound_buffer((path / "sounds" / "auto_move.wav").string()));
}

void tetris::game::render_grid() {

    float vertical_step   = 2.f / static_cast<float>(state::board_size.first);
    float horizontal_step = 2.f / static_cast<float>(state::board_size.second);

    for (float i = -1.f; i <= 1.f; i += horizontal_step) {
        eng::line line;
        line.a.x = -1;
        line.a.y = i;

        line.b.x = 1;
        line.b.y = i;

        engine->render_line(line);
    }

    for (float i = -1.f; i <= 1.f; i += vertical_step) {
        eng::line line;
        line.a.x = i;
        line.a.y = -1;

        line.b.x = i;
        line.b.y = 1;

        engine->render_line(line);
    }
}

void tetris::game::render_board() {
    // init renderer
    if (rstate.rows_destroyed > 0) {
        if (!rstate.initialized) {
            rstate.initialize(engine->time_from_init(),
                              engine->time_from_init() + step_time);
        }
        if (current_piece)
            current_piece->render(engine);
    }
    // if rendereded, reset
    if (rstate.rows_destroyed > 0 && rstate.finished)
        rstate.reset();
    // render piece
    if (current_piece)
        current_piece->render(engine);
    // render board
    rstate.render(engine, engine->time_from_init());
    engine->swap_buffers();
    // render_grid();
}

// returns RAW POINTER
tetris::piece* tetris::game::generate_piece() {
    auto type = static_cast<piece_types>(pieces_distribution(random_engine));

    auto   col = static_cast<color>(color_distribution(random_engine));
    piece* ptr;

    switch (type) {
        case piece_types::O:
            ptr = new O{};
            break;
        case piece_types::I:
            ptr = new I{};
            break;
        case piece_types::J:
            ptr = new J{};
            break;
        case piece_types::L:
            ptr = new L{};
            break;
        case piece_types::T:
            ptr = new T{};
            break;
        case piece_types::S:
            ptr = new S{};
            break;
        case piece_types::Z:
            ptr = new Z{};
            break;
        default:
            throw std::logic_error("Invalid enum class value");
    }
    ptr->set_color(col);
    return ptr;
}

void tetris::game::fixate(tetris::piece* ptr) {
    for (auto p : ptr->get_tiles()) {
        field[(p.coords.first + (state::board_size.first * p.coords.second))] =
            new tile{ p };
        rstate.full_state[get_field_index(p.coords.first, p.coords.second)] = p;
        if (p.coords.second > state::board_size.second)
            lost = true;
    }
}

void tetris::game::play() {
    auto time = engine->time_from_init();
    sounds["start"]->play(eng::sound_buffer::properties::once);
    while (!lost) {
        eng::event event;
        while (engine->read_input(event)) {
            switch (event) {
                case eng::event::esc_pressed:
                    lost = true;
                    break;
                case eng::event::w_pressed:
                    if (current_piece) {
                        auto tiles{ current_piece->coords_after_rotation() };
                        for (const auto& tile : tiles) {
                            if (!within_filed(tile.first, tile.second))
                                goto exit;
                            if (get_tile(tile.first, tile.second) != nullptr)
                                goto exit;
                        }
                        sounds["rotate"]->play(
                            eng::sound_buffer::properties::once);
                        current_piece->rotate();
                    }
                    break;
                case eng::event::s_pressed:
                    if (current_piece) {
                        if (movable(current_piece, { 0, -1 })) {
                            current_piece->move(event);
                            // sounds["move"]->play(
                            //     eng::sound_buffer::properties::once);
                        }
                    }
                    break;
                case eng::event::d_pressed:
                    if (current_piece) {
                        if (current_piece) {
                            if (movable(current_piece, { 1, 0 })) {
                                current_piece->move(event);
                                // sounds["move"]->play(
                                //     eng::sound_buffer::properties::once);
                            }
                        }
                        break;
                        case eng::event::a_pressed:
                            if (current_piece) {
                                if (movable(current_piece, { -1, 0 })) {
                                    current_piece->move(event);
                                    // sounds["move"]->play(
                                    // eng::sound_buffer::properties::once);
                                }
                            }
                            break;
                    }
                default:
                exit:
                    break;
            }
        }
        if (time + step_time < engine->time_from_init()) {
            time = engine->time_from_init();
            round();
            if (current_piece) {
                if (movable(current_piece, { 0, -1 })) {
                    current_piece->move(eng::event::s_pressed);
                    sounds["move"]->play(eng::sound_buffer::properties::once);
                }
            }
        }
        // clear bord if any row is full
        clear(0);
        if (rstate.rows_destroyed > 0)
            sounds["clear"]->play(eng::sound_buffer::properties::once);
        render_board();

        score.update();
        step_time = 1.0 - (0.1 * score.level) - 0.1;
    }
    std::cout << "score: " << score.score << '\n'
              << "level: " << score.level << '\n';
}

void tetris::game::round() {
    if (current_piece == nullptr)
        current_piece = generate_piece();

    for (const auto& tile : current_piece->get_tiles()) {

        if (tile.coords.second == 0) {
            fixate(current_piece);
            delete current_piece;
            current_piece = nullptr;
            return;
        }

        if ((field.at((tile.coords.first +
                       (state::board_size.first * (tile.coords.second - 1)))) !=
             nullptr)) {
            fixate(current_piece);
            delete current_piece;
            current_piece = nullptr;
            return;
        }
    }
}

void tetris::game::shift_down(int bottom_row) {
    // for each row
    for (int i = bottom_row; i < state::board_size.second; i++) {
        //   for each row cell
        for (int x = 0; x < state::board_size.first; x++) {
            // if not empty
            if (field.at(x + (i * state::board_size.first)) != nullptr) {
                // move tile coords
                field.at(x + (i * state::board_size.first))->move_down();
                // change board coords
                field.at(x + ((i - 1) * state::board_size.first)) =
                    new tetris::tile{ *(
                        field.at(x + (i * state::board_size.first))) };
                delete field.at(x + (i * state::board_size.first));
                field.at(x + (i * state::board_size.first)) = nullptr;
            }
        }
    }
}

bool tetris::game::is_full(int row) {
    for (int x = 0; x < state::board_size.first; x++) {
        if (field.at(x + (state::board_size.first * row)) == nullptr)
            return false;
    }
    return true;
}

void tetris::game::clear_row(int row) {
    if (rstate.initialized)
        throw std::logic_error(
            "rows removed before previous animation has finished");
    // lower threshold above which tiles should be interpolated if needed
    if (rstate.threshhold_value > get_field_index(0, row))
        rstate.threshhold_value = get_field_index(0, row);
    // increment -y displacement to render
    rstate.rows_destroyed++;
    for (int x = 0; x < state::board_size.first; x++) {
        rstate.tiles_to_delete[get_field_index(x, row + rstate.adjustment)] =
            true;
        delete (field.at(x + (state::board_size.first * row)));
        field.at(x + (state::board_size.first * row)) = nullptr;
    }
    rstate.adjustment++;
    score.lines_cleared++;
}

// recursive
void tetris::game::clear(int row) {
    for (int i = row; i < state::board_size.second; i++) {
        if (is_full(i)) {
            clear_row(i);
            shift_down(i + 1);
            clear(i);
            return;
        }
    }
}

tetris::tile*& tetris::game::get_tile(int x, int y) {
    return field.at(x + (y * state::board_size.first));
}

bool tetris::game::movable(tetris::piece*&            piece,
                           const std::pair<int, int>& displacement) {
    for (auto& tile : piece->get_tiles()) {
        if (!within_filed(tile.coords.first + displacement.first,
                          tile.coords.second + displacement.second))
            return false;
        if (get_tile(tile.coords.first + displacement.first,
                     tile.coords.second + displacement.second) != nullptr)
            return false;
    }
    return true;
}

void tetris::game_logic::update() {
    if (lines_cleared == 0)
        return;
    score += (40 * lines_cleared) * lines_cleared;
    lines_cleared_total += lines_cleared;
    if (lines_cleared >= 5) {
        level++;
        lines_cleared = 0;
    }
}