(load (format nil "~a/libgi/step-gen.lisp" cm-cmdline:top_srcdir))

(pragma once)

(namespace wf
	   ;; TODO framebuffer should not be attached to rays
  (defstep initialize_framebuffer :id "initialize framebuffer" :data ((RD rd raydata)))
  (defstep download_framebuffer   :id "download framebuffer"   :data ((RD rd raydata)))

  (defstep sample_camera_rays
		  :id "sample camera rays"
                  :data ((RD rd raydata)))
  
  (defstep add_hitpoint_albedo
		  :id "add hitpoint albedo"
                  :data ((RD sample_rays raydata)))

  (defstep find_closest_hits :ifs nil :data ((RD rd raydata)) :run (rt->use rd))
  (defstep find_any_hits     :ifs nil :data ((RD rd raydata)) :run (rt->use rd))
)

;; vim: set ft=lisp :
