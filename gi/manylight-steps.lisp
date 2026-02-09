(load (format nil "~a/libgi/step-gen.lisp" cm-cmdline:top_srcdir))

(pragma once)
(include "direct-steps.h")
(namespace wf
  (comment "Should support camdata and bouncedata being aliases")
  (defstep manylight_step
           :id "manylight step"
           :data ((RD camdata raydata) (RD bouncedata raydata) (PDF pdf (per-sample float))))
)

;; vim: set ft=lisp :