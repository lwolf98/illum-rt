(load (format nil "~a/libgi/step-gen.lisp" cm-cmdline:top_srcdir))

(pragma once)

(namespace wf
	   ;; TODO framebuffer should not be attached to rays
  (defstep initialize_framebuffer :id "initialize framebuffer" :data ((RD rd raydata)))
  (comment "This should be combined with the initialize_framebuffer step")
  (defstep download_framebuffer   :id "download framebuffer"   :data ((RD rd raydata)))
  (defstep copy_to_preview        :id "copy to preview"        :data ((RD rd raydata)))

  (defstep sample_camera_rays
          :id "sample camera rays"
          :data ((RD rd raydata)))

  (defstep add_hitpoint_albedo
          :id "add hitpoint albedo"
          :data ((RD sample_rays raydata)))

  (comment "This should be combined with the add_hitpoint_albedo step")
  (defstep add_hitpoint_albedo_to_framebuffer
          :id "add hitpoint albedo secondary"
          :data ((RD sample_rays raydata)))

  (defstep add_hitpoint_normal_to_framebuffer
          :id "add hitpoint normal secondary"
          :data ((RD sample_rays raydata)))

  (defstep find_closest_hits :ifs nil :data ((RD rd raydata)) :run (rt->use rd))
  (defstep find_any_hits     :ifs nil :data ((RD rd raydata)) :run (rt->use rd))
)

;; vim: set ft=lisp :
