//
// Created by kil3 on 3/15/25.
//

#ifndef GAMESTATE_H
#define GAMESTATE_H


struct GameState {
    inline static bool game_session_in_progress_ = false;
    inline static bool refresh_session_ = false;
    inline static bool game_finished_ = false;
};


#endif //GAMESTATE_H
