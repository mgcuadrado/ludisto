#ifndef SXAKO_GENERAL_FIDE_HEADER_
#define SXAKO_GENERAL_FIDE_HEADER_

#include "piece_game.h"
#include "think.h"

namespace sxako::fide {

  using namespace piece_2d_game;

  struct LimitedRepetition { using this_t=LimitedRepetition;
    LimitedRepetition(Piece2DGameData &d,
                      size_t max_past_size,
                      series<Square> irreversible_squares);
    Piece2DGameData const &d;
    size_t const max_past_size;
    series<Square> const irreversible_squares;
    Addressing<Kind::manag, SingleVar, size_t> const past_size;
    using hash_t=size_t;
    Addressing<Kind::manag, Straight<size_t>, hash_t> const past_hash;
  private:
    void move_handler(Board &b, Location from, Location to) const;
    void turn_handler(Board &b) const;
    void outcome_filter(Rules::Outcome &o, Board const &b) const;
    void reset(Board &b) const;
  };

  // check for checkmate and stalemate
  struct Mate { using this_t=Mate;
    Mate(Piece2DGameData &d,
         TrackPiece const &track_kings,
         PerSquareLegalMovesFilter const &per_square_legal_moves_filter);
    Piece2DGameData const &d;
    TrackPiece const &track_kings;
    PerSquareLegalMovesFilter const &per_square_legal_moves_filter;
  private:
    void outcome_filter(Rules::Outcome &o, Board const &b) const;
  };

  struct DrawIfRoyalsAlone { using this_t=DrawIfRoyalsAlone;
    DrawIfRoyalsAlone(Piece2DGameData &d,
                      series<Square> royal_squares);
    Piece2DGameData const &d;
    series<Square> const royal_squares;
  private:
    void outcome_filter(Rules::Outcome &o, Board const &b) const;
  };

  struct AvoidCheck { using this_t=AvoidCheck;
    AvoidCheck(Piece2DGameData &d,
               TrackPiece const &track_kings,
               PerSquareLegalMovesFilter &per_square_legal_moves_filter);
    Piece2DGameData const &d;
    TrackPiece const &track_kings;
    PerSquareLegalMovesFilter const &per_square_legal_moves_filter;
  private:
    bool in_check(Board const &b, Move m) const;
  };

  // regular FIDE displacements
  series<Displacement> const
    w_i_pawn_advance_steps{{0, +1}, {0, +2}},
    b_i_pawn_advance_steps{{0, -1}, {0, -2}},
    w_pawn_advance_steps{{0, +1}},
    b_pawn_advance_steps{{0, -1}},
    w_pawn_capture_steps={{-1, +1}, {+1, +1}},
    b_pawn_capture_steps={{-1, -1}, {+1, -1}},
    king_steps=
      {{-1, +1}, { 0, +1}, {+1, +1},
       {-1,  0},           {+1,  0},
       {-1, -1}, { 0, -1}, {+1, -1}},
    queen_steps=king_steps,
    knight_steps=
         {{-1, +2}, {+1, +2},
       {-2, +1},       {+2, +1},
       {-2, -1},       {+2, -1},
          {-1, -2}, {+1, -2}},
    bishop_steps=
      {{-1, +1}, {+1, +1},
       {-1, -1}, {+1, -1}},
    rook_steps=
           {{ 0, +1},
       {-1,  0}, {+1,  0},
            { 0, -1}};

  // the following move function implements the essence of pawnness: can't
  // capture advancing, but only diagonally; on first move may advance more
  // than one step; at the end of its advancing journey, promotes; en-passant
  // is implemented by "EnPassant"
  using promotion_f=std::function<series<Square> (Board const &, Location)>;
  using PawnSpecs=std::tuple<series<Displacement>,
                             series<Displacement>,
                             promotion_f>;
  PawnSpecs pawn_specs(series<Displacement> advance_steps,
                       series<Displacement> capture_steps,
                       promotion_f promotion=promotion_f());
  void move_pawn(Piece2DGameData const &d,
                 // "spec" is a tuple: a series of displacements is
                 // used for advancing; a second one is used for capturing; a
                 // function that determines promotion options based on ending
                 // location:
                 PawnSpecs const &pawn_specs,
                 add_legal_move_f const &add_move,
                 Board const &b, Location l,
                 Square l_s, Square change_to, // same: no change
                 bool only_capture, Location to_capture);

  struct EnPassant { using this_t=EnPassant;
    EnPassant(Piece2DGameData &d,
              PerSquareLegalMovesFilter &per_square_legal_moves_filter,
              MoveCommandHandler &move_command_handler,
              series<Square> jumping_squares,
              series<std::pair<Square, series<Displacement>>>
              capturing_squares_steps);
    Piece2DGameData const &d;
    series<Square> const jumping_squares;
    series<std::pair<Square, series<Displacement>>> const
      capturing_squares_steps;
    series<Displacement> const capturing_steps;
    Addressing<Kind::state, SingleVar, bool> const on;
    Addressing<Kind::state, SingleVar, Location> const started, ended;
  private:
    void move_handler(Board &b, Location from, Location to) const;
    void legal_moves_filter(add_legal_move_f const &add_move,
                            Board const &b,
                            Location l,
                            series<Displacement> const &capturing_steps) const;
    void reset(Board &b) const;
  };

  // king castling, compatible with Fide chess and Chess960
  struct KingCastling { using this_t=KingCastling;
    KingCastling(
        Piece2DGameData &d,
        series<std::tuple<Square, Square, Square, Square>>
          initial_and_moved_king_and_initial_and_moved_rook);
    Piece2DGameData const &d;
    PerSquareLegalMovesFilter const &per_square_legal_moves_filter;
  private:
    void legal_moves_filter(
         add_legal_move_f const &add_move,
         Board const &b,
         Location l,
         Square i_king, Square m_king, Square i_rook, Square m_rook,
         bool only_capture) const;
  };

  struct Move_i_rook_king_and_castling : Move_base {
    using Move_base::Move_base;
    SimpleMoves
      i_rook_m{
        per_square,
        {{{pb["w_i_rook"], pb["w_rook"]}, {pb["b_i_rook"], pb["b_rook"]}}},
        straight, rook_steps},
      i_king_m{
        per_square,
        {{{pb["w_i_king"], pb["w_king"]}, {pb["b_i_king"], pb["b_king"]}}},
        one_step, king_steps};
    KingCastling king_castling{
        d,
        {{pb["w_i_king"], pb["w_king"], pb["w_i_rook"], pb["w_rook"]},
         {pb["b_i_king"], pb["b_king"], pb["b_i_rook"], pb["b_rook"]}}};
  };

  struct Move_rook_knight_bishop_queen : Move_base {
    using Move_base::Move_base;
    SimpleMoves
        rook_m  {per_square, {pb["w_rook"],   pb["b_rook"]  },
                 straight, rook_steps  },
        knight_m{per_square, {pb["w_knight"], pb["b_knight"]},
                 one_step, knight_steps},
        bishop_m{per_square, {pb["w_bishop"], pb["b_bishop"]},
                 straight, bishop_steps},
        queen_m {per_square, {pb["w_queen"],  pb["b_queen"] },
                 straight, queen_steps };
  };

  struct Move_king : Move_base {
    using Move_base::Move_base;
    SimpleMoves
      king_m{per_square, {pb["w_king"], pb["b_king"]}, one_step, king_steps};
  };
  struct Move_king_and_check_mate : Move_king {
    using Move_king::Move_king;
    TrackPiece track_kings{d, d.move_command_handler, 'k'};
    AvoidCheck avoid_check{d, track_kings, per_square};
    Mate mate{d, track_kings, per_square};
  };

  struct Move_pawns : Move_base {
    Move_pawns(Piece2DGameData &d,
               std::array<promotion_f, 2> promotions)
      : Move_base(d), promotions(promotions) { }
    std::array<promotion_f, 2> const promotions;
    static auto const white=0, black=1;
    SimpleMoves
      w_p_m{per_square, pb["w_pawn"], move_pawn,
            pawn_specs(w_pawn_advance_steps, w_pawn_capture_steps,
                       promotions[white])},
      b_p_m{per_square, pb["b_pawn"], move_pawn,
            pawn_specs(b_pawn_advance_steps, b_pawn_capture_steps,
                       promotions[black])};
  };
  struct Move_pawns_i_pawns : Move_pawns {
    Move_pawns_i_pawns(
      Piece2DGameData &d,
      std::array<promotion_f, 2> promotions,
      std::array<series<Displacement>, 2> i_pawn_advance_steps)
      : Move_pawns(d, promotions),
        i_pawn_advance_steps(i_pawn_advance_steps) { }
    std::array<series<Displacement>, 2> const i_pawn_advance_steps;
    SimpleMoves
      w_i_p_m{per_square, {{{pb["w_i_pawn"], pb["w_pawn"]}}}, move_pawn,
              pawn_specs(i_pawn_advance_steps[white], w_pawn_capture_steps,
                         promotions[white])},
      b_i_p_m{per_square, {{{pb["b_i_pawn"], pb["b_pawn"]}}}, move_pawn,
              pawn_specs(i_pawn_advance_steps[black], b_pawn_capture_steps,
                         promotions[black])};
  };
  struct Move_pawns_i_pawns_en_passant : Move_pawns_i_pawns {
    using Move_pawns_i_pawns::Move_pawns_i_pawns;
    EnPassant en_passant{d, per_square, d.move_command_handler,
                         {pb["w_i_pawn"], pb["b_i_pawn"]},
                         {{pb["w_pawn"], w_pawn_capture_steps},
                          {pb["b_pawn"], b_pawn_capture_steps}}};
  };

  /// evaluation
  using material_table_t=std::map<Piece, score_t>;

  score_t just_material(Piece2DGameData const &d, Board const &b,
                        material_table_t const &material_table);
  class EvaluationJustMaterial
    : public Evaluation {
  public:
    EvaluationJustMaterial(EvaluationMap &evaluation_map,
                           Piece2DGameData const &d,
                           material_table_t const &material_table);
  };

  score_t material_and_position(Piece2DGameData const &d, Board const &b,
                                material_table_t const &material_table,
                                float factor);
  class EvaluationMaterialAndPosition
    : public Evaluation {
  public:
    EvaluationMaterialAndPosition(EvaluationMap &evaluation_map,
                                  Piece2DGameData const &d,
                                  material_table_t const &material_table);
  };

}

#endif
