
#include <glue/text.h>

#include <array>
#include <thread>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <numeric>
#include <mutex>
#include <bitset>

namespace glue {

tf_atlas::tf_atlas(size_t font_size)
        : font_size(font_size), pos(), size(), rowh(0), index(), font_tex() {
  int w = 1024;
  int h = static_cast<int>(font_size) * 2;

  glActiveTexture(GL_TEXTURE0);

  font_tex = texture::make();
  font_tex.bind(GL_TEXTURE_2D);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
  texture::pixel_store(GL_UNPACK_ALIGNMENT, 1);

  texture::parameter(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  texture::parameter(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  texture::parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  texture::parameter(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  size.width = w;
  size.height = h;
}

const glyph_tf& tf_atlas::operator[](char32_t ch) const {
  for (const glyph_tf &info : index) {
    if (info.ch == ch) {
      return info;
    }
  }

  throw std::runtime_error("Could not find char");
}

void tf_atlas::add_glyphs(const std::vector<glyph_bmp> &glyphs) {

  glActiveTexture(GL_TEXTURE0);
  font_tex.bind(GL_TEXTURE_2D);

  for(const glyph_bmp &bmp : glyphs) {

    //"line wrap"
    if (pos.x + bmp.width + 1 >= size.width) {
      pos.y += rowh;
      rowh = bmp.height;
      pos.x = 0;
    } else {
      rowh = std::max(rowh, bmp.height);
    }

    if ((pos.y + rowh) > size.height) {
      std::vector<uint8_t> data(size.height * size.width);
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());

      size.height += (pos.y + rowh);
      data.resize((size.height * size.width));

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, size.width, size.height,
                   0, GL_RED, GL_UNSIGNED_BYTE, data.data());
      //std::cerr << "Increasing tf atlas size: " << size.width << " " << size.height << std::endl;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    static_cast<int>(pos.x),
                    static_cast<int>(pos.y),
                    static_cast<int>(bmp.width),
                    static_cast<int>(bmp.height),
                    GL_RED, GL_UNSIGNED_BYTE,
                    bmp.bytes.data());

    index.emplace_back(bmp, pos.x, pos.y);

    pos.x += bmp.width + 1.0f;
  }

}


tf_atlas tf_font::make_atlas(size_t size, const std::string &characters) {
  std::vector<glyph_bmp> glyphs = glyphs_for(size, characters);
  tf_atlas the_atlas(size);
  the_atlas.add_glyphs(glyphs);
  return the_atlas;
}


tf_atlas& tf_font::atlas_for_size(size_t size) {
  auto iter = atlases->find(size);

  if (iter == atlases->end()) {
    std::string str(126 - 32, '\0');
    std::iota(str.begin(), str.end(), 32);
    str += u8"π\u00b0\ufffd";

    tf_atlas atlas = make_atlas(size, str);
    auto res = atlases->insert(std::make_pair(size, atlas));
    iter = res.first;
  }

  return iter->second;
}


std::vector<glyph_bmp> tf_font::glyphs_for(size_t size, const std::string &u8str) {
  FT_Set_Pixel_Sizes(face, 0, size);
  FT_GlyphSlot g = face->glyph;

  std::u32string u32str = u8to32(u8str);

  std::vector<glyph_bmp> glyphs;
  for (const char32_t ch : u32str) {
    if (FT_Load_Char(face, ch, FT_LOAD_RENDER)) {
      fprintf(stderr, "Loading character %c failed!\n", ch);
      continue;
    }

    glyph_bmp bmp;
    bmp.ch = ch;
    bmp.ax = g->advance.x >> 6;
    bmp.ay = g->advance.y >> 6;

    bmp.width = g->bitmap.width;
    bmp.height = g->bitmap.rows;
    bmp.left = g->bitmap_left;
    bmp.top = g->bitmap_top;

    size_t nbytes = static_cast<size_t>(bmp.width * bmp.height);
    bmp.bytes.resize(nbytes, 0);
    memcpy(bmp.bytes.data(), g->bitmap.buffer, nbytes);

    glyphs.emplace_back(std::move(bmp));
  }

  return glyphs;
}

FT_Library tf_font::ft_library() {
  static FT_Library lib;
  static std::once_flag flag;

  std::call_once(flag, [](){
      std::cout << "Init freetype!" << std::endl;
      if (FT_Init_FreeType(&lib)) {
        throw std::runtime_error("Could not init freetype library");
      }
  });

  return lib;
}



// misc

ssize_t u8_nbytes(uint8_t bytes) {
  std::bitset<8> bs(bytes);

  size_t i;
  for (i = 0; i < bs.size(); i++) {
    if (bs[7 - i] == 0) {
      break;
    }
  }

  switch (i) {            // 0123 4567
    case 0:  return  1; // 0??? ????
    case 1:  return -1; // 10?? ????
    case 7:  return -1; // 1111 1110
    case 8:  return -1; // 1111 1111
    default: return  i; // 1... .10?
  }
}

template<typename Iter>
Iter u8to32(Iter begin, Iter end, char32_t &out) {
  if (begin == end) {
    out = 0xFFFD;
    return end;
  }

  auto ix = *begin++;
  uint8_t c1;
  memcpy(&c1, &ix, 1);

  ssize_t nb = u8_nbytes(c1);
  if (nb == 1) {
    out = c1;
    return begin;
  } else if (nb < 1) {
    //TODO: synchronize
    out = 0xFFFD;
    return begin;
  }

  // nb > 1
  out = c1 & (0xFFU >> (nb+1));
  out = out << 6*(nb - 1);

  size_t len = static_cast<size_t>(nb);
  size_t i;
  for (i = 1; begin != end && i < len; i++) {
    ix = *begin++;
    uint8_t ch;
    memcpy(&ch, &ix, 1);

    if ((ch & 0xC0) != 0x80) {
      //todo: synchronize
      out = 0xFFFD;
      return begin;
    }

    out |= (ch & 0x3F) << 6*(nb - i - 1);
  }

  if (i != len) {
    // incomplete character
    out = 0xFFFD;
  }

  return begin;
}

// utility functins
std::u32string u8to32(const std::string &istr) {
  std::u32string res;

  if (istr.empty()) {
    return res;
  }

  auto iter = istr.cbegin();
  auto iend = istr.cend();

  while (iter != iend) {
    char32_t ch;
    iter = u8to32(iter, iend, ch);
    res.push_back(ch);
  }

  return res;
}


} // glue::