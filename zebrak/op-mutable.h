// Copyright 2009 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// A set of basic operands, especially useful for fixed-point arithmetic, with
// fast ASM implementations.


//#define USE_OPTIMIZED_OP
#ifdef USE_OPTIMIZED_OP

typedef union {
  uint16_t value;
  uint8_t bytes[2];
} Word;

static inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance) {
  Word sum;
  asm(
    "mul %3, %2"      "\n\t"  // b * balance
    "movw %A0, r0"    "\n\t"  // to sum
    "com %2"          "\n\t"  // 255 - balance
    "mul %1, %2"      "\n\t"  // a * (255 - balance)
    "com %2"          "\n\t"  // reset balance to its previous value
    "add %A0, r0"     "\n\t"  // add to sum L
    "adc %B0, r1"     "\n\t"  // add to sum H
    "eor r1, r1"      "\n\t"  // reset r1 after multiplication
    : "&=r" (sum)
    : "a" (a), "a" (balance), "a" (b)
    );
  return sum.bytes[1];
}

static inline uint8_t InterpolateSample(
    const uint8_t *table,
    uint16_t phase) {
  uint8_t result;
  uint8_t work;
  asm(
    "movw r30, %A2"           "\n\t"  // copy base address to r30:r31
    "add r30, %B3"            "\n\t"  // increment table address by phaseH
    "adc r31, r1"             "\n\t"  // just carry
    "mov %1, %A3"             "\n\t"  // move phaseL to working register
    "lpm %0, z+"              "\n\t"  // load sample[n]
    "lpm r1, z+"              "\n\t"  // load sample[n+1]
    "mul %1, r1"              "\n\t"  // multiply second sample by phaseL
    "movw r30, r0"            "\n\t"  // result to accumulator
    "com %1"                  "\n\t"  // 255 - phaseL -> phaseL
    "mul %1, %0"              "\n\t"  // multiply first sample by phaseL
    "add r30, r0"             "\n\t"  // accumulate L
    "adc r31, r1"             "\n\t"  // accumulate H
    "eor r1, r1"              "\n\t"  // reset r1 after multiplication
    "mov %0, r31"             "\n\t"  // use sum H as output
    : "=r" (result), "=r" (work)
    : "r" (table), "r" (phase)
    : "r30", "r31"
  );
  return result;
}

static inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b) {
  uint8_t result;
  asm(
    "mul %1, %2"      "\n\t"
    "mov %0, r1"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int8_t S8U8MulShift8(int8_t a, uint8_t b) {
  uint8_t result;
  asm(
    "mulsu %1, %2"    "\n\t"
    "mov %0, r1"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S8U8Mul(int8_t a, uint8_t b) {
  int16_t result;
  asm(
    "mulsu %1, %2"    "\n\t"
    "movw %0, r0"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int16_t S8S8Mul(int8_t a, int8_t b) {
  int16_t result;
  asm(
    "muls %1, %2"    "\n\t"
    "movw %0, r0"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline uint16_t U8U8Mul(uint8_t a, uint8_t b) {
  uint16_t result;
  asm(
    "mul %1, %2"    "\n\t"
    "movw %0, r0"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline int8_t S8S8MulShift8(int8_t a, int8_t b) {
  uint8_t result;
  asm(
    "muls %1, %2"    "\n\t"
    "mov %0, r1"      "\n\t"
    "eor r1, r1"      "\n\t"
    : "=r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}

static inline uint16_t U16U8MulShift8(uint16_t a, uint8_t b) {
  uint16_t result;
  asm(
    "eor %B0, %B0"    "\n\t"
    "mul %A1, %A2"    "\n\t"
    "mov %A0, r1"     "\n\t"
    "mul %B1, %A2"  "\n\t"
    "add %A0, r0"     "\n\t"
    "adc %B0, r1"     "\n\t"
    "eor r1, r1"      "\n\t"
    : "=&r" (result)
    : "a" (a), "a" (b)
  );
  return result;
}


#else

static inline uint8_t U8U8MulShift8(uint8_t a, uint8_t b) {
  return a * b >> 8;
}

static inline int8_t S8U8MulShift8(int8_t a, uint8_t b) {
  return a * b >> 8;
}

static inline int16_t S8U8Mul(int8_t a, uint8_t b) {
  return a * b;
}

static inline int16_t S8S8Mul(int8_t a, int8_t b) {
  return a * b;
}

static inline uint16_t U8U8Mul(uint8_t a, uint8_t b) {
  return a * b;
}

static inline int8_t S8S8MulShift8(int8_t a, int8_t b) {
  return a * b >> 8;
}

static inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance) {
  return (a * (255 - balance) + b * balance) >> 8;
}

uint8_t S8Mix(int8_t a, int8_t b, uint8_t balance) {
  return S8U8MulShift8(a, 255 - balance) + S8U8MulShift8(b, balance);
}

static inline uint16_t U16U8MulShift8(uint16_t a, uint8_t b) {
  return ((uint32_t)a * (uint32_t)b) >> 8;
}


static inline uint8_t InterpolateSampleU8(
    const uint8_t *table,
    uint16_t phase) {
  return U8Mix(
      pgm_read_byte(table + (phase >> 8)),
      pgm_read_byte(1 + table + (phase >> 8)),
      phase & 0xff);
}

static inline int8_t InterpolateSampleS8(
    const int8_t *table,
    uint16_t phase) {
#ifdef WAVE_INTERPOLATION
  return S8Mix(
      pgm_read_byte(table + (phase >> 8)),
      pgm_read_byte(1 + table + (phase >> 8)),
      phase & 0xff);
#else
      return pgm_read_byte(table + (phase >> 8));
#endif
}

static inline uint16_t U16S16AddSat(uint16_t a, int16_t b)
{
	uint16_t c = a + b;
	if (b > 0) {
		if (c < a)
			return 0xffff;
		return c;
	} else {
		if (c > a)
			return 0;
		return c;
	}
}

static inline uint16_t U16U16AddSat(uint16_t a, uint16_t b)
{
	uint16_t c = a + b;
	if (c < a)
		return 0xffff;
	return c;
}

static inline uint16_t U16U16SubSat(uint16_t a, uint16_t b)
{
	if (a < b)
		return 0;
	return a - b;
}


#endif

static inline uint16_t InterpolateIncreasingU16(
    const uint16_t *table,
    uint16_t phase) {
  uint16_t a = pgm_read_word(table + (phase >> 8));
  uint16_t b = pgm_read_word(table + (phase >> 8) + 1);
  return a + U16U8MulShift8(b - a, phase);
}
