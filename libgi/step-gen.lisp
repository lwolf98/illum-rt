(defmacro defstep (name &key id data (ifs t) run)
  `(progn
     ,(cl:if ifs
	 `(class ',name ((public step))
	    (public
	      (decl ((static constexpr char id[] = ,id)))
	      ,(cl:if data
		 `(function use ,(loop for (nil name basetype) in data collect `(,basetype (dref ',name))) pure -> void)))))
     ,(cl:if data
	 `(namespace wire
	    (template ,(remove-duplicates (loop for (type nil nil) in data collect `(typename ',type))
					  :from-end t :test #'equal)
	       (class ',name ((public (from-namespace wf ',name)))
		 (public
		   (using (from-namespace wf ',name ',name))
		   (decl ,(loop for (type name nil) in data
		                collect `(',type (dref ',name) = nullptr))  ;; dref is a stupid hack
		     (function properly-wired () -> bool
		       (return (&& ,@(loop for (nil name nil) in data collect name))))
		     (function use ,(loop for (nil name basetype) in data collect `(,basetype (dref ',name))) -> void
		       ,@(loop for (type name nil) in data
			       collect `(set (pref this ',name) (dynamic-cast ',type (dref nil) ',name)))) ;; dref nil! :/
		     ,(cl:if run `(function run () override -> void
				    ,run
				    (funcall (from-namespace wf ',name run))))))))))))


(defmacro per-sample (type)
  `(instantiate per_sample_data (,type)))
