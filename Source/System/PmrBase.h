#pragma once

struct Object {};

struct PmrBase: Object { virtual ~PmrBase() noexcept = default; };
