(load (format nil "~a/libgi/step-gen.lisp" cm-cmdline:top_srcdir))

(pragma once)
(include "primary-steps.h")

(namespace wf
  (comment "Should support camdata and bouncedata being aliases")
  (defstep sample_uniform_dir
           :id "sample uniform dir"
           :data ((RD camdata raydata) (RD bouncedata raydata) (PDF pdf (per-sample float))))
  (defstep sample_cos_weighted_dir
           :id "sample cosine distributed dir"
           :data ((RD camdata raydata) (RD bouncedata raydata) (PDF pdf (per-sample float))))
  (defstep integrate_light_sample
           :id "integrate light sample"
           :data ((RD camrays raydata) (RD shadowrays raydata) (PDF pdf (per-sample float))))
)

;; vim: set ft=lisp :
