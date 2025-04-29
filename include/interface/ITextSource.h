//
// Created by Kristijan Zalac on 3/4/25.
//

#ifndef ITEXTSOURCE_H
#define ITEXTSOURCE_H

#include <string>


class ITextSource {
    public:
        virtual ~ITextSource() = default;
        [[nodiscard]] virtual std::string get_text() const = 0;
};


#endif //ITEXTSOURCE_H
