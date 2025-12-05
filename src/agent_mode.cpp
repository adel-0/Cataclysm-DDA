#include "agent_mode.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "creature_tracker.h"
#include "cursesdef.h"
#include "game.h"
#include "input.h"
#include "json.h"
#include "map.h"
#include "messages.h"
#include "monster.h"
#include "weather.h"

#if defined(TILES) || defined(_WIN32)
#include "cursesport.h"
#endif

namespace agent
{

void write_screen( JsonOut &jout )
{
    jout.start_object();

#if defined(TILES) || defined(_WIN32)
    // Access the curses window buffer directly
    cata_cursesport::WINDOW *win = catacurses::stdscr.get<cata_cursesport::WINDOW>();
    if( win ) {
        jout.member( "width", win->width );
        jout.member( "height", win->height );
        jout.member( "cursor_x", win->cursor.x );
        jout.member( "cursor_y", win->cursor.y );

        // Write lines as array of strings
        jout.member( "lines" );
        jout.start_array();
        for( int y = 0; y < win->height && y < static_cast<int>( win->line.size() ); ++y ) {
            std::string line_str;
            const cata_cursesport::curseline &line = win->line[y];
            for( int x = 0; x < win->width && x < static_cast<int>( line.chars.size() ); ++x ) {
                const std::string &ch = line.chars[x].ch;
                if( ch.empty() ) {
                    line_str += ' ';
                } else {
                    line_str += ch;
                }
            }
            jout.write( line_str );
        }
        jout.end_array();

        // Write colors as 2D array of [fg, bg] pairs
        jout.member( "colors" );
        jout.start_array();
        for( int y = 0; y < win->height && y < static_cast<int>( win->line.size() ); ++y ) {
            jout.start_array();
            const cata_cursesport::curseline &line = win->line[y];
            for( int x = 0; x < win->width && x < static_cast<int>( line.chars.size() ); ++x ) {
                jout.start_array();
                jout.write( static_cast<int>( line.chars[x].FG ) );
                jout.write( static_cast<int>( line.chars[x].BG ) );
                jout.end_array();
            }
            jout.end_array();
        }
        jout.end_array();
    } else {
        jout.member( "width", 0 );
        jout.member( "height", 0 );
        jout.member( "lines" );
        jout.start_array();
        jout.end_array();
        jout.member( "colors" );
        jout.start_array();
        jout.end_array();
    }
#else
    // For ncurses builds, we can't easily access the buffer
    // Fall back to dimensions only
    jout.member( "width", catacurses::getmaxx( catacurses::stdscr ) );
    jout.member( "height", catacurses::getmaxy( catacurses::stdscr ) );
    jout.member( "lines" );
    jout.start_array();
    jout.end_array();
    jout.member( "colors" );
    jout.start_array();
    jout.end_array();
#endif

    jout.end_object();
}

void write_game_state( JsonOut &jout )
{
    jout.start_object();

    // Only write game state if game is initialized
    if( g ) {
        avatar &u = get_avatar();

        // Player info
        jout.member( "player" );
        jout.start_object();
        jout.member( "name", u.get_name() );

        // Position
        jout.member( "position" );
        jout.start_object();
        tripoint pos = u.pos();
        jout.member( "x", pos.x );
        jout.member( "y", pos.y );
        jout.member( "z", pos.z );
        jout.end_object();

        // Basic stats
        jout.member( "moves", u.get_moves() );
        jout.member( "speed", u.get_speed() );
        jout.member( "stamina", u.get_stamina() );
        jout.member( "stamina_max", u.get_stamina_max() );

        jout.end_object();  // player

        // Time
        jout.member( "turn", to_turns<int>( calendar::turn - calendar::turn_zero ) );

        // Recent messages
        jout.member( "messages" );
        jout.start_array();
        std::vector<std::pair<std::string, std::string>> msgs = Messages::recent_messages( 10 );
        for( const auto &msg : msgs ) {
            jout.start_object();
            jout.member( "turn", msg.first );
            jout.member( "text", msg.second );
            jout.end_object();
        }
        jout.end_array();

        // Visible creatures
        jout.member( "visible_creatures" );
        jout.start_array();
        for( Creature &critter : g->all_creatures() ) {
            if( &critter != &u && u.sees( critter ) ) {
                jout.start_object();
                jout.member( "name", critter.get_name() );
                tripoint critter_pos = critter.pos();
                jout.member( "position" );
                jout.start_object();
                jout.member( "x", critter_pos.x );
                jout.member( "y", critter_pos.y );
                jout.member( "z", critter_pos.z );
                jout.end_object();

                // Relative position to player
                jout.member( "relative" );
                jout.start_object();
                jout.member( "x", critter_pos.x - pos.x );
                jout.member( "y", critter_pos.y - pos.y );
                jout.member( "z", critter_pos.z - pos.z );
                jout.end_object();

                jout.end_object();
            }
        }
        jout.end_array();
    }

    jout.end_object();
}

void send_observation( JsonOut &jout, input_context &ctx )
{
    jout.start_object();

    jout.member( "type", "observation" );
    jout.member( "context", ctx.get_category() );

    // Valid actions for this context
    jout.member( "valid_actions" );
    jout.start_array();
    for( const std::string &action : ctx.get_registered_actions() ) {
        jout.write( action );
    }
    jout.end_array();

    // Screen content
    jout.member( "screen" );
    write_screen( jout );

    // Game state
    jout.member( "game_state" );
    write_game_state( jout );

    jout.end_object();
}

std::string wait_for_input( input_context &ctx )
{
    // Send observation to stdout
    JsonOut jout( std::cout );
    send_observation( jout, ctx );
    std::cout << std::endl;  // Newline and flush

    // Read command from stdin
    std::string line;
    if( !std::getline( std::cin, line ) ) {
        // EOF or error
        return std::string();
    }

    // Parse JSON command
    try {
        std::istringstream iss( line );
        TextJsonIn jsin( iss );
        TextJsonObject cmd = jsin.get_object();

        std::string action = cmd.get_string( "action" );

        // Validate action is registered in this context
        const std::vector<std::string> &valid = ctx.get_registered_actions();
        if( std::find( valid.begin(), valid.end(), action ) == valid.end() ) {
            // Invalid action - send error
            JsonOut jerr( std::cout );
            jerr.start_object();
            jerr.member( "type", "error" );
            jerr.member( "message", "Invalid action: " + action );
            jerr.end_object();
            std::cout << std::endl;
            return std::string();
        }

        return action;
    } catch( const std::exception &e ) {
        // JSON parse error - send error response
        JsonOut jerr( std::cout );
        jerr.start_object();
        jerr.member( "type", "error" );
        jerr.member( "message", std::string( "JSON parse error: " ) + e.what() );
        jerr.end_object();
        std::cout << std::endl;
        return std::string();
    }
}

} // namespace agent
