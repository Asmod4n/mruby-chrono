#include <mruby.h>
#ifdef MRB_NO_FLOAT
#error "cant be build without Float class"
#endif
#ifdef MRB_USE_FLOAT32
#error "MRB_USE_FLOAT32 is too small for mruby-chrono"
#endif
#include <mruby/data.h>
#include <mruby/class.h>
#include <chrono>
#include <mruby/cpp_helpers.hpp>
#include <mruby/branch_pred.h>
#include <new>

using namespace std::chrono;

static mrb_value
mrb_chrono_steady_now(mrb_state *mrb, mrb_value self)
{
  auto t = steady_clock::now().time_since_epoch();
  return mrb_float_value(mrb, duration<mrb_float>(t).count());
}

static mrb_value
mrb_chrono_system_now(mrb_state *mrb, mrb_value self)
{
  auto t = system_clock::now().time_since_epoch();
  return mrb_float_value(mrb, duration<mrb_float>(t).count());
}

struct Timer {
  steady_clock::time_point start;

  Timer() : start(steady_clock::now()) {}

  void reset() {
    this->start = steady_clock::now();
  }

  mrb_float elapsed() const {
    auto now = steady_clock::now();
    return duration<mrb_float>(now - this->start).count();
  }
};

MRB_CPP_DEFINE_TYPE(Timer, Timer)

static mrb_value
timer_init(mrb_state* mrb, mrb_value self)
{
  mrb_cpp_new<Timer>(mrb, self);

  return self;
}

static mrb_value timer_reset(mrb_state* mrb, mrb_value self)
{
  Timer* t = static_cast<Timer*>(DATA_PTR(self));
  if(unlikely(!t)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Timer not initialized");
  }
  t->reset();
  return self;
}

static mrb_value timer_elapsed(mrb_state* mrb, mrb_value self)
{
  Timer* t = static_cast<Timer*>(DATA_PTR(self));
  if(unlikely(!t)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Timer not initialized");
  }
  return mrb_float_value(mrb, t->elapsed());
}

MRB_BEGIN_DECL
void
mrb_mruby_chrono_gem_init(mrb_state* mrb)
{
  struct RClass *chrono_mod, *steady_mod, *system_mod;
  chrono_mod = mrb_define_module(mrb, "Chrono");
  mrb_define_module_function(mrb, chrono_mod, "steady", mrb_chrono_steady_now, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, chrono_mod, "system", mrb_chrono_system_now, MRB_ARGS_NONE());

  steady_mod = mrb_define_module_under(mrb, chrono_mod, "Steady");
  mrb_define_module_function(mrb, steady_mod, "now", mrb_chrono_steady_now, MRB_ARGS_NONE());

  system_mod = mrb_define_module_under(mrb, chrono_mod, "System");
  mrb_define_module_function(mrb, system_mod, "now", mrb_chrono_system_now, MRB_ARGS_NONE());

  struct RClass* timer_cls = mrb_define_class_under(mrb, chrono_mod, "Timer", mrb->object_class);
  MRB_SET_INSTANCE_TT(timer_cls, MRB_TT_DATA);
  mrb_define_method(mrb, timer_cls, "initialize", timer_init, MRB_ARGS_NONE());
  mrb_define_method(mrb, timer_cls, "reset", timer_reset, MRB_ARGS_NONE());
  mrb_define_method(mrb, timer_cls, "elapsed", timer_elapsed, MRB_ARGS_NONE());
}

void mrb_mruby_chrono_gem_final(mrb_state* mrb) {}
MRB_END_DECL