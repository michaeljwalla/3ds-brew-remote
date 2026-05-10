#include "datatypes.h"
#include "Inputs.h"

class InputMouse: public InputObject {
    using coordinate = coords<int16_t>;
    coordinate pos;
    InputMouse(ObjectID id, std::string_view name);
    
    void init() override;
    public:
        void button_down(int btn);
        void button_up(int btn);
        void button_click(int btn);
        void move(coordinate delta);
        void set_pos(coordinate newPos);
        coordinate get_pos() const;


    friend class InputController;

};