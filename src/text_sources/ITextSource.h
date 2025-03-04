//
// Created by kil3 on 3/4/25.
//

#ifndef ITEXTSOURCE_H
#define ITEXTSOURCE_H
#include <iostream>
#include <string>


class ITextSource {
    public:
        virtual ~ITextSource() = default;
        virtual std::string get_text() const = 0;
};


#endif //ITEXTSOURCE_H
